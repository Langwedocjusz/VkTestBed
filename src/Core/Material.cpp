#include "Material.h"

const MaterialKey Material::Albedo = 
	 MaterialKey{"Albedo", MaterialValueType::Vec3};
const MaterialKey Material::Normal = 
	 MaterialKey{"Normal", MaterialValueType::Vec3};
const MaterialKey Material::Roughness = 
	 MaterialKey{"Roughness", MaterialValueType::Float};
const MaterialKey Material::Metallic = 
	 MaterialKey{"Metallic", MaterialValueType::Float};
const MaterialKey Material::AlphaCutoff = 
	 MaterialKey{"AlphaCutoff", MaterialValueType::Float};