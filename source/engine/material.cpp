#include "material.h"

bool Material::is_ready() const
{
	if (shading_model == ShadingModel::Skybox)
		return albedo.is_ready();

	return albedo.is_ready()
		&& normal.is_ready()
		&& roughness.is_ready()
		&& metalness.is_ready();
}

void Material::create_descriptor_set(yar_shader* shader, yar_sampler* sampler)
{
	if (descriptor_set != nullptr)
		return;

	yar_descriptor_set_desc set_desc{};
	set_desc.max_sets = 1;
	set_desc.shader = shader;
	set_desc.update_freq = yar_update_freq_none;
	add_descriptor_set(&set_desc, &descriptor_set);

	yar_texture_type tex_type = (shading_model == ShadingModel::Skybox)
		? yar_texture_type_cube_map
		: yar_texture_type_2d;

	std::vector<yar_descriptor_info> infos;

	if (shading_model == ShadingModel::Skybox)
	{
		infos = {
			{
				.name = "skybox_map",
				.descriptor = yar_descriptor_info::yar_combined_texture_sample{
					get_gpu_texture(albedo, tex_type),
					"samplerState",
				}
			},
			{
				.name = "samplerState",
				.descriptor = sampler
			}
		};
	}
	else
	{
		infos = {
			{
				.name = "diffuse_map",
				.descriptor = yar_descriptor_info::yar_combined_texture_sample{
					get_gpu_texture(albedo, yar_texture_type_2d),
					"samplerState",
				}
			},
			{
				.name = "roughness_map",
				.descriptor = yar_descriptor_info::yar_combined_texture_sample{
					get_gpu_texture(roughness, yar_texture_type_2d, 1),
					"samplerState",
				}
			},
			{
				.name = "metalness_map",
				.descriptor = yar_descriptor_info::yar_combined_texture_sample{
					get_gpu_texture(metalness, yar_texture_type_2d, 1),
					"samplerState",
				}
			},
			{
				.name = "normal_map",
				.descriptor = yar_descriptor_info::yar_combined_texture_sample{
					get_gpu_texture(normal, yar_texture_type_2d, 3),
					"samplerState",
				}
			},
			{
				.name = "samplerState",
				.descriptor = sampler
			}
		};
	}

	yar_update_descriptor_set_desc update_desc{};
	update_desc.index = 0;
	update_desc.infos = std::move(infos);
	update_descriptor_set(&update_desc, descriptor_set);
}
