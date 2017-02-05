#include "../assert.hpp"
#include "../drawers.hpp"
#include "../game_constants.hpp"
#include "../log.hpp"
#include "../math_utils.hpp"
#include "../messages_extractor.inl"
#include "../sound/sound_engine.hpp"
#include "../sound/sound_id.hpp"

#include "client.hpp"

namespace PanzerChasm
{

Client::Client(
	Settings& settings,
	const GameResourcesConstPtr& game_resources,
	const MapLoaderPtr& map_loader,
	const RenderingContext& rendering_context,
	const DrawersPtr& drawers,
	const Sound::SoundEnginePtr& sound_engine,
	const DrawLoadingCallback& draw_loading_callback )
	: game_resources_(game_resources)
	, map_loader_(map_loader)
	, sound_engine_(sound_engine)
	, draw_loading_callback_(draw_loading_callback)
	, current_tick_time_( Time::CurrentTime() )
	, camera_controller_(
		settings,
		m_Vec3( 0.0f, 0.0f, 0.0f ),
		float(rendering_context.viewport_size.Width()) / float(rendering_context.viewport_size.Height()) )
	, map_drawer_( settings, game_resources, rendering_context )
	, weapon_state_( game_resources )
	, hud_drawer_( game_resources, rendering_context, drawers )
{
	PC_ASSERT( game_resources_ != nullptr );
	PC_ASSERT( map_loader_ != nullptr );

	std::memset( &player_state_, 0, sizeof(player_state_) );
}

Client::~Client()
{}

void Client::SetConnection( IConnectionPtr connection )
{
	if( connection == nullptr )
		connection_info_= nullptr;
	else
		connection_info_.reset( new ConnectionInfo( connection ) );
}

bool Client::Disconnected() const
{
	if( connection_info_ == nullptr )
		return true;

	return connection_info_->connection->Disconnected();
}

void Client::ProcessEvents( const SystemEvents& events )
{
	using KeyCode= SystemEvent::KeyEvent::KeyCode;

	for( const SystemEvent& event : events )
	{
		if( event.type == SystemEvent::Type::MouseKey &&
			event.event.mouse_key.mouse_button == 1u )
		{
			shoot_pressed_= event.event.mouse_key.pressed;
		}
		else if( event.type == SystemEvent::Type::MouseMove )
		{
			camera_controller_.ControllerRotate(
				event.event.mouse_move.dy,
				event.event.mouse_move.dx );
		}

		// Select weapon.
		if( event.type == SystemEvent::Type::Key && event.event.key.pressed )
		{
			if( event.event.key.key_code >= KeyCode::K1 &&
				static_cast<unsigned int>(event.event.key.key_code) < static_cast<unsigned int>(KeyCode::K1) + GameConstants::weapon_count )
			{
				unsigned int weapon_index=
					static_cast<unsigned int>( event.event.key.key_code ) - static_cast<unsigned int>( KeyCode::K1 );

				if( player_state_.ammo[ weapon_index ] > 0u &&
					( player_state_.weapons_mask & (1u << weapon_index) ) != 0u )
					requested_weapon_index_= weapon_index;
			}
		}
	} // for events
}

void Client::Loop( const KeyboardState& keyboard_state )
{
	current_tick_time_= Time::CurrentTime();

	if( connection_info_ != nullptr )
		connection_info_->messages_extractor.ProcessMessages( *this );

	camera_controller_.Tick( keyboard_state );

	if( sound_engine_ != nullptr )
	{
		sound_engine_->SetHeadPosition(
			player_position_ + m_Vec3( 0.0f, 0.0f, GameConstants::player_eyes_level ), // TODO - use exact camera position here
			camera_controller_.GetViewAngleZ(),
			camera_controller_.GetViewAngleX() );

		if( map_state_ != nullptr )
			sound_engine_->UpdateMapState( *map_state_ );
	}

	hud_drawer_.SetPlayerState( player_state_, weapon_state_.CurrentWeaponIndex() );

	if( player_state_.ammo[ requested_weapon_index_ ] == 0u )
		TrySwitchWeaponOnOutOfAmmo();

	if( map_state_ != nullptr )
		map_state_->Tick( current_tick_time_ );

	if( connection_info_ != nullptr )
	{
		float move_direction, move_acceleration;
		camera_controller_.GetAcceleration( keyboard_state, move_direction, move_acceleration );

		Messages::PlayerMove message;
		message.view_direction= AngleToMessageAngle( camera_controller_.GetViewAngleZ() + Constants::half_pi );
		message.move_direction= AngleToMessageAngle( move_direction );
		message.acceleration= static_cast<unsigned char>( move_acceleration * 254.5f );
		message.jump_pressed= camera_controller_.JumpPressed();
		message.weapon_index= requested_weapon_index_;

		message.view_dir_angle_x= AngleToMessageAngle( camera_controller_.GetViewAngleX() );
		message.view_dir_angle_z= AngleToMessageAngle( camera_controller_.GetViewAngleZ() );
		message.shoot_pressed= shoot_pressed_;

		connection_info_->messages_sender.SendUnreliableMessage( message );
		connection_info_->messages_sender.Flush();
	}
}

void Client::Draw()
{
	if( map_state_ != nullptr )
	{
		m_Vec3 pos= player_position_;
		const float z_shift= camera_controller_.GetEyeZShift();

		if( player_state_.health > 0u )
			pos.z+= GameConstants::player_eyes_level + z_shift;
		else
			pos.z= std::min( pos.z + GameConstants::player_deathcam_level, GameConstants::walls_height );

		m_Mat4 view_rotation_and_projection_matrix, projection_matrix;
		camera_controller_.GetViewRotationAndProjectionMatrix( view_rotation_and_projection_matrix );
		camera_controller_.GetViewProjectionMatrix( projection_matrix );

		map_drawer_.Draw(
			*map_state_,
			view_rotation_and_projection_matrix,
			pos,
			player_state_.health > 0u ? player_monster_id_ : 0u /* draw body, if death */ );

		// Draw weapon, if alive.
		if( player_state_.health > 0u )
		{
			const float c_weapon_shift_scale= 0.25f;
			const float c_weapon_angle_scale= 0.75f;
			const float weapon_angle_for_shift= camera_controller_.GetViewAngleX() *  c_weapon_angle_scale;
			const float weapon_shift_z= c_weapon_shift_scale * z_shift * std::cos( weapon_angle_for_shift );
			const float weapon_shift_y= c_weapon_shift_scale * z_shift * std::sin( weapon_angle_for_shift );

			m_Mat4 weapon_shift_matrix;
			weapon_shift_matrix.Translate( m_Vec3( 0.0f, weapon_shift_y, weapon_shift_z ) );

			map_drawer_.DrawWeapon(
				weapon_state_,
				weapon_shift_matrix * projection_matrix,
				pos,
				camera_controller_.GetViewAngleX(),
				camera_controller_.GetViewAngleZ() );
		}

		hud_drawer_.DrawCrosshair();
		hud_drawer_.DrawCurrentMessage( current_tick_time_ );
		hud_drawer_.DrawHud( map_mode_ );
	}
}

void Client::operator()( const Messages::MessageBase& message )
{
	PC_ASSERT(false);
	Log::Warning( "Unknown message for server: ", int(message.message_id) );
}

void Client::operator()( const Messages::PlayerSpawn& message )
{
	MessagePositionToPosition( message.xyz, player_position_ );
	camera_controller_.SetAngles( MessageAngleToAngle( message.direction ), 0.0f );
	player_monster_id_= message.player_monster_id;
}

void Client::operator()( const Messages::PlayerPosition& message )
{
	MessagePositionToPosition( message.xyz, player_position_ );
	camera_controller_.SetSpeed( MessageCoordToCoord( message.speed ) );
}

void Client::operator()( const Messages::PlayerState& message )
{
	player_state_= message;
}

void Client::operator()( const Messages::PlayerWeapon& message )
{
	weapon_state_.ProcessMessage( message );
}

void Client::operator()( const Messages::MapEventSound& message )
{
	if( sound_engine_ != nullptr )
	{
		m_Vec3 pos;
		MessagePositionToPosition( message.xyz, pos );
		sound_engine_->PlayWorldSound( message.sound_id, pos );
	}
}

void Client::operator()( const Messages::MonsterLinkedSound& message )
{
	if( sound_engine_ == nullptr )
		return;

	if( message.monster_id == player_monster_id_ )
	{
		sound_engine_->PlayHeadSound( message.sound_id );
	}
	else if( map_state_ != nullptr )
	{
		const MapState::MonstersContainer& monsters= map_state_->GetMonsters();
		const auto it= monsters.find( message.monster_id );
		if( it != monsters.end() )
			sound_engine_->PlayMonsterLinkedSound( *it, message.sound_id );
	}
}

void Client::operator()( const Messages::MonsterSound& message )
{
	if( sound_engine_ == nullptr )
		return;

	if( map_state_ != nullptr )
	{
		const MapState::MonstersContainer& monsters= map_state_->GetMonsters();
		const auto it= monsters.find( message.monster_id );
		if( it != monsters.end() )
			sound_engine_->PlayMonsterSound( *it, message.monster_sound_id );
	}
}

void Client::operator()( const Messages::MapChange& message )
{
	const auto show_progress=
	[&]( const float progress )
	{
		if( draw_loading_callback_ != nullptr )
			draw_loading_callback_( progress, "Client" );
	};

	show_progress( 0.0f );

	const MapDataConstPtr map_data= map_loader_->LoadMap( message.map_number );
	if( map_data == nullptr )
	{
		// TODO - handel error
		PC_ASSERT(false);
		return;
	}

	show_progress( 0.333f );
	map_drawer_.SetMap( map_data );

	show_progress( 0.666f );
	map_state_.reset( new MapState( map_data, game_resources_, Time::CurrentTime() ) );

	show_progress( 0.8f );
	if( sound_engine_ != nullptr )
		sound_engine_->SetMap( map_data );

	hud_drawer_.ResetMessage();

	current_map_data_= map_data;

	show_progress( 1.0f );
}

void Client::operator()( const Messages::TextMessage& message )
{
	if( current_map_data_ != nullptr )
	{
		if( message.text_message_number < current_map_data_->messages.size() )
		{
			hud_drawer_.AddMessage( current_map_data_->messages[ message.text_message_number ], current_tick_time_ );

			// TODO - start playing text message sound from server, as monster-linked sound.
			if( sound_engine_ != nullptr )
				sound_engine_->PlayHeadSound( Sound::SoundId::Message );
		}
	}
}

void Client::TrySwitchWeaponOnOutOfAmmo()
{
	for( unsigned int i= 1u; i < GameConstants::weapon_count; i++ )
	{
		if(
			( player_state_.weapons_mask & ( 1u << i ) ) != 0u &&
			player_state_.ammo[i] != 0u )
		{
			requested_weapon_index_= i;
			return;
		}
	}

	requested_weapon_index_= 0u;
}

} // namespace PanzerChasm
