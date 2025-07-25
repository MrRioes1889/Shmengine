#pragma once

#include "Defines.hpp"
#include "systems/MaterialSystem.hpp"

struct TextureMapResourceData
{
	char name[Constants::max_texture_name_length];
	char texture_name[Constants::max_texture_name_length];
	TextureFilter::Value filter_min;
	TextureFilter::Value filter_mag;
	TextureRepeat::Value repeat_u;
	TextureRepeat::Value repeat_v;
	TextureRepeat::Value repeat_w;
};

struct MaterialResourceData
{
	char name[Constants::max_material_name_length];
	char shader_name[Constants::max_shader_name_length];

	MaterialType type;
	bool8 auto_unload;

    Darray<MaterialProperty> properties;
	Darray<TextureMapResourceData> maps;
};

namespace ResourceSystem
{
	bool8 material_loader_load(const char* name, MaterialResourceData* out_config);
	void material_loader_unload(MaterialResourceData* config);

	bool8 material_loader_import_obj_material_library_file(const char* file_path);
}

