#include <cstring>
#include <sstream>

#include "assert.hpp"
#include "log.hpp"

#include "game_resources.hpp"

namespace PanzerChasm
{

static void LoadItemsDescription(
	const Vfs::FileContent& inf_file,
	GameResources& game_resources )
{
	const char* const items_start=
		std::strstr( reinterpret_cast<const char*>(inf_file.data()), "[3D_OBJECTS]" );
	const char* const items_end= reinterpret_cast<const char*>(inf_file.data()) + inf_file.size() - 1u;

	std::istringstream stream( std::string( items_start, items_end ) );

	char line[ 512 ];
	stream.getline( line, sizeof(line), '\n' );

	unsigned int items_count= 0u;
	stream >> items_count;
	stream.getline( line, sizeof(line), '\n' );

	game_resources.items_description.resize( items_count );
	for( unsigned int i= 0u; i < items_count; )
	{
		stream.getline( line, sizeof(line), '\n' );
		if( line[0] == ';' )
			continue;

		std::istringstream line_stream{ std::string( line ) };

		GameResources::ItemDescription& item_description= game_resources.items_description[i];

		line_stream >> item_description.radius; // GoRad
		line_stream >> item_description.cast_shadow; // Shad
		line_stream >> item_description.bmp_obj; // BObj
		line_stream >> item_description.bmp_z; // BMPz
		line_stream >> item_description.a_code; // AC
		line_stream >> item_description.blow_up; // Blw
		line_stream >> item_description.b_limit; // BLmt
		line_stream >> item_description.b_sfx; // BSfx
		line_stream >> item_description.sfx; // SFX

		line_stream >> item_description.model_file_name; // FileName

		item_description.animation_file_name[0]= '\0';
		line_stream >> item_description.animation_file_name; // Animation

		i++;
	}
}

static void LoadMonstersDescription(
	const Vfs::FileContent& inf_file,
	GameResources& game_resources )
{
	const char* const monsters_start=
		std::strstr( reinterpret_cast<const char*>(inf_file.data()), "[MONSTERS]" );
	const char* const monsters_end= reinterpret_cast<const char*>(inf_file.data()) + inf_file.size() - 1u;

	std::istringstream stream( std::string( monsters_start, monsters_end ) );

	char line[ 512 ];
	stream.getline( line, sizeof(line), '\n' );

	unsigned int monsters_count= 0u;
	stream >> monsters_count;
	stream.getline( line, sizeof(line), '\n' );

	game_resources.monsters_description.resize( monsters_count );

	for( unsigned int i= 0u; i < monsters_count; )
	{
		stream.getline( line, sizeof(line), '\n' );
		if( line[0] == ';' )
			continue;

		std::istringstream line_stream{ std::string( line ) };

		GameResources::MonsterDescription& monster_description= game_resources.monsters_description[i];

		line_stream >> monster_description.model_file_name;
		line_stream >> monster_description.w_radius; // WRad
		line_stream >> monster_description.a_radius; // ARad
		line_stream >> monster_description.speed; // SPEED/
		line_stream >> monster_description.r; // R
		line_stream >> monster_description.life; // LIFE
		line_stream >> monster_description.kick; // Kick
		line_stream >> monster_description.rock; // Rock
		line_stream >> monster_description.sep_limit; // SepLimit

		i++;
	}
}

static void LoadSpriteEffectsDescription(
	const Vfs::FileContent& inf_file,
	GameResources& game_resources )
{
	const char* const effects_start=
		std::strstr( reinterpret_cast<const char*>(inf_file.data()), "[BLOWS]" );
	const char* const effects_end= reinterpret_cast<const char*>(inf_file.data()) + inf_file.size() - 1u;

	std::istringstream stream( std::string( effects_start, effects_end ) );

	char line[ 512 ];
	stream.getline( line, sizeof(line), '\n' );

	unsigned int effects_count= 0u;
	stream >> effects_count;
	stream.getline( line, sizeof(line), '\n' );

	game_resources.sprites_effects_description.resize( effects_count );

	for( unsigned int i= 0u; i < effects_count; )
	{
		stream.getline( line, sizeof(line), '\n' );
		if( line[0] == ';' )
			continue;

		std::istringstream line_stream{ std::string( line ) };

		GameResources::SpriteEffectDescription& effect_description= game_resources.sprites_effects_description[i];

		line_stream >> effect_description.glass;
		line_stream >> effect_description.half_size;
		line_stream >> effect_description.smooking;
		line_stream >> effect_description.looped;
		line_stream >> effect_description.gravity;
		line_stream >> effect_description.jump;
		line_stream >> effect_description.light_on;
		line_stream >> effect_description.sprite_file_name;

		i++;
	}
}

static void LoadWeaponsDescription(
	const Vfs::FileContent& inf_file,
	GameResources& game_resources )
{
	const char* const weapons_start=
		std::strstr( reinterpret_cast<const char*>(inf_file.data()), "[WEAPONS]" );
	const char* const weapons_end= reinterpret_cast<const char*>(inf_file.data()) + inf_file.size() - 1u;

	std::istringstream stream( std::string( weapons_start, weapons_end ) );

	char line[ 512 ];
	stream.getline( line, sizeof(line), '\n' );

	unsigned int weapon_count= 0u;
	stream >> weapon_count;
	stream.getline( line, sizeof(line), '\n' );

	game_resources.weapons_description.resize( weapon_count );

	for( unsigned int i= 0u; i < weapon_count; i++ )
	{
		GameResources::WeaponDescription& weapon_description= game_resources.weapons_description[i];

		for( unsigned int j= 0u; j < 3u; j++ )
		{
			stream.getline( line, sizeof(line), '\n' );

			const char* param_start= line;
			while( std::isspace( *param_start ) ) param_start++;

			const char* param_end= param_start;
			while( std::isalpha( *param_end ) ) param_end++;

			const char* value_start= param_end;
			while( std::isspace( *value_start ) ) value_start++;
			value_start++;
			while( std::isspace( *value_start ) ) value_start++;

			const char* value_end= value_start;
			while( !std::isspace( *value_end ) ) value_end++;

			const unsigned int param_length= param_end - param_start;
			const unsigned int value_length= value_end - value_start;

			if( std::strncmp( param_start, "MODEL", param_length ) == 0 )
				std::strncpy( weapon_description.model_file_name, value_start, value_length );
			else if( std::strncmp( param_start, "STAT", param_length ) == 0 )
				std::strncpy( weapon_description.animation_file_name, value_start, value_length );
			else if( std::strncmp( param_start, "SHOOT", param_length ) == 0 )
				std::strncpy( weapon_description.reloading_animation_file_name, value_start, value_length );
		}

		stream.getline( line, sizeof(line), '\n' );
		std::istringstream line_stream{ std::string( line ) };

		line_stream >> weapon_description.r_type;
		line_stream >> weapon_description.reloading_time;
		line_stream >> weapon_description.y_sh;
		line_stream >> weapon_description.r_z0;
		line_stream >> weapon_description.d_am;
		line_stream >> weapon_description.limit;
		line_stream >> weapon_description.r_count;
	}
}

static void LoadItemsModels(
	const Vfs& vfs,
	GameResources& game_resources )
{
	game_resources.items_models.resize( game_resources.items_description.size() );

	Vfs::FileContent file_content;
	Vfs::FileContent animation_file_content;

	for( unsigned int i= 0u; i < game_resources.items_models.size(); i++ )
	{
		const GameResources::ItemDescription& item_description= game_resources.items_description[i];

		vfs.ReadFile( item_description.model_file_name, file_content );

		if( item_description.animation_file_name[0u] != '\0' )
			vfs.ReadFile( item_description.animation_file_name, animation_file_content );
		else
			animation_file_content.clear();

		LoadModel_o3( file_content, animation_file_content, game_resources.items_models[i] );
	}
}

static void LoadEffectsSprites(
	const Vfs& vfs,
	GameResources& game_resources )
{
	game_resources.effects_sprites.resize( game_resources.sprites_effects_description.size() );

	Vfs::FileContent file_content;

	for( unsigned int i= 0u; i < game_resources.effects_sprites.size(); i++ )
	{
		vfs.ReadFile( game_resources.sprites_effects_description[i].sprite_file_name, file_content );
		LoadObjSprite( file_content, game_resources.effects_sprites[i] );
	}
}

static void LoadWeaponsModels(
	const Vfs& vfs,
	GameResources& game_resources )
{
	game_resources.weapons_models.resize( game_resources.weapons_description.size() );

	Vfs::FileContent file_content;
	Vfs::FileContent animation_file_content[2u];

	for( unsigned int i= 0u; i < game_resources.weapons_models.size(); i++ )
	{
		const GameResources::WeaponDescription& weapon_description= game_resources.weapons_description[i];

		vfs.ReadFile( weapon_description.model_file_name, file_content );
		vfs.ReadFile( weapon_description.animation_file_name, animation_file_content[0] );
		vfs.ReadFile( weapon_description.reloading_animation_file_name, animation_file_content[1] );

		LoadModel_o3( file_content, animation_file_content, 2u, game_resources.weapons_models[i] );
	}
}

GameResourcesConstPtr LoadGameResources( const VfsPtr& vfs )
{
	PC_ASSERT( vfs != nullptr );

	const GameResourcesPtr result= std::make_shared<GameResources>();

	result->vfs= vfs;

	LoadPalette( *vfs, result->palette );

	const Vfs::FileContent inf_file= vfs->ReadFile( "CHASM.INF" );

	if( inf_file.empty() )
		Log::FatalError( "Can not read CHASM.INF" );

	LoadItemsDescription( inf_file, *result );
	LoadMonstersDescription( inf_file, *result );
	LoadSpriteEffectsDescription( inf_file, *result );
	LoadWeaponsDescription( inf_file, *result );

	LoadItemsModels( *vfs, *result );
	LoadEffectsSprites( *vfs, *result );
	LoadWeaponsModels( *vfs, *result );

	return result;
}

} // namespace PanzerChasm
