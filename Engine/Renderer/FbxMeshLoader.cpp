#include "MeshPrimitives.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Core/VFS/AssetPathResolver.h"

#pragma warning(push)
#pragma warning(disable: 4244 4267 4456 4457 4701 4702)
#include <ufbx.h>
#pragma warning(pop)

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Core::Renderer
{
	static std::string ToLower(std::string_view s)
	{
		std::string result;
		result.reserve(s.size());
		for (char c : s)
		{
			result.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
		}
		return result;
	}

	static std::string DeriveTextureStem(const std::string& filename)
	{
		std::filesystem::path p(filename);
		std::string stem = p.stem().string();
		std::string lower = ToLower(stem);

		if (lower.ends_with("_diffuse"))
		{
			stem = stem.substr(0, stem.length() - 8);
		}
		else if (lower.ends_with("_albedo"))
		{
			stem = stem.substr(0, stem.length() - 7);
		}
		else if (lower.ends_with("_basecolor"))
		{
			stem = stem.substr(0, stem.length() - 10);
		}
		return stem;
	}

	MeshData LoadFbxMesh(const std::string& filePath)
	{
		std::string resolvedPath = filePath;
		auto resolved = Core::AssetPathResolver::resolve(filePath);

		ufbx_load_opts opts{};
		opts.target_axes = ufbx_axes_left_handed_y_up;
		opts.target_unit_meters = 1.0f;
		ufbx_error error{};
		ufbx_scene* scene = nullptr;
		std::vector<uint8_t> fbxBytes;

		if (resolved.has_value() && resolved->isVFS)
		{
			auto binRes = Core::AssetPathResolver::readBinary(filePath);
			if (binRes.isOk())
			{
				fbxBytes = std::move(binRes.value());
				scene = ufbx_load_memory(fbxBytes.data(), fbxBytes.size(), &opts, &error);
			}
			else
			{
				DF_LOG_ERROR("Renderer", "Failed to read binary VFS FBX asset '{}': {}", filePath, binRes.error());
			}
		}
		else
		{
			if (resolved.has_value() && !resolved->isVFS)
			{
				resolvedPath = resolved->physicalPath.string();
			}
			else if (!std::filesystem::exists(resolvedPath))
			{
				if (std::filesystem::exists("Assets/" + filePath))
				{
					resolvedPath = "Assets/" + filePath;
				}
				else if (std::filesystem::exists("Templates/GraphicsTest3D/Assets/Models/Sponza/" + std::filesystem::path(filePath).filename().string()))
				{
					resolvedPath = "Templates/GraphicsTest3D/Assets/Models/Sponza/" + std::filesystem::path(filePath).filename().string();
				}
			}
			scene = ufbx_load_file(resolvedPath.c_str(), &opts, &error);
		}

		if (!scene)
		{
			DF_LOG_ERROR("Renderer", "ufbx load failed for '{}': {}", filePath, error.description.data);
			return CreateCubeMesh(1.0f);
		}

		std::filesystem::path resolvedP(resolvedPath);
		std::filesystem::path fbxDir = resolvedP.parent_path();
		std::string assetParent = std::filesystem::path(filePath).parent_path().generic_string();
		std::string parentVirtualDir = (resolved.has_value() && resolved->isVFS)
			? std::filesystem::path(resolved->virtualPath).parent_path().generic_string()
			: assetParent;
		while (parentVirtualDir.starts_with("/"))
		{
			parentVirtualDir = parentVirtualDir.substr(1);
		}
		if (parentVirtualDir.empty())
		{
			parentVirtualDir = assetParent;
			while (parentVirtualDir.starts_with("/"))
			{
				parentVirtualDir = parentVirtualDir.substr(1);
			}
		}

		std::unordered_map<std::string, std::string> dirLookup;
		if (!resolvedPath.empty() && std::filesystem::exists(fbxDir))
		{
			for (const auto& entry : std::filesystem::directory_iterator(fbxDir))
			{
				if (entry.is_regular_file())
				{
					std::string fname = entry.path().filename().string();
					std::string lowerFname = ToLower(fname);
					std::string relativePath = assetParent.empty() ? fname : (assetParent + "/" + fname);
					dirLookup[lowerFname] = relativePath;
				}
			}
		}

		auto findInLookup = [&](const std::string& name) -> std::string {
			if (name.empty()) return "";
			auto it = dirLookup.find(ToLower(name));
			if (it != dirLookup.end()) return it->second;

			std::string candidate = parentVirtualDir.empty() ? name : (parentVirtualDir + "/" + name);
			auto vfsRes = Core::AssetPathResolver::resolve(candidate);
			if (vfsRes.has_value())
			{
				return candidate;
			}
			return "";
		};

		MeshData data;

		for (size_t n = 0; n < scene->nodes.count; ++n)
		{
			const ufbx_node* node = scene->nodes.data[n];
			if (!node || !node->mesh) continue;

			const ufbx_mesh* mesh = node->mesh;
			ufbx_real det = ufbx_matrix_determinant(&node->geometry_to_world);
			bool flipWinding = (det < 0.0f) ^ mesh->reversed_winding;
			ufbx_matrix normMat = ufbx_matrix_for_normals(&node->geometry_to_world);
			{
				// ufbx_matrix_for_normals computes the unnormalized 3x3 cofactor matrix whose
				// entries scale by s^2 (e.g. 1e-4 for 0.01 cm-to-m FBX node scale), causing
				// LengthSquared() to be ~1e-8. Rescale normMat so transformed normals stay O(1).
				double c0 = normMat.m00 * normMat.m00 + normMat.m10 * normMat.m10 + normMat.m20 * normMat.m20;
				double c1 = normMat.m01 * normMat.m01 + normMat.m11 * normMat.m11 + normMat.m21 * normMat.m21;
				double c2 = normMat.m02 * normMat.m02 + normMat.m12 * normMat.m12 + normMat.m22 * normMat.m22;
				double maxColSq = std::max({ c0, c1, c2 });
				if (maxColSq > 1e-30)
				{
					ufbx_real invScale = static_cast<ufbx_real>(1.0 / std::sqrt(maxColSq));
					for (size_t k = 0; k < 9; ++k)
					{
						normMat.v[k] *= invScale;
					}
				}
			}

			for (size_t p = 0; p < mesh->material_parts.count; ++p)
			{
				const ufbx_mesh_part& part = mesh->material_parts.data[p];
				if (part.num_triangles == 0) continue;

				Submesh sub{};
				sub.startIndexLocation = static_cast<uint32_t>(data.indices.size());
				sub.baseVertexLocation = 0;

				const ufbx_material* mat = (part.index < node->materials.count) ? node->materials.data[part.index] : nullptr;
				std::string matName = mat ? std::string(mat->name.data, mat->name.length) : ("submesh_" + std::to_string(part.index));
				sub.name = matName;

				std::vector<uint32_t> triIndices(mesh->max_face_triangles * 3);
				std::vector<uint32_t> partVertexMap(mesh->num_indices, UINT32_MAX);
				Math::Vector3 subMin(1e30f, 1e30f, 1e30f);
				Math::Vector3 subMax(-1e30f, -1e30f, -1e30f);
				uint32_t subFirstVert = static_cast<uint32_t>(data.vertices.size());

				for (size_t fi = 0; fi < part.face_indices.count; ++fi)
				{
					uint32_t faceIndex = part.face_indices.data[fi];
					ufbx_face face = mesh->faces.data[faceIndex];
					uint32_t numTris = ufbx_triangulate_face(triIndices.data(), triIndices.size(), mesh, face);

					for (uint32_t t = 0; t < numTris; ++t)
					{
						uint32_t c0 = triIndices[t * 3 + 0];
						uint32_t c1 = flipWinding ? triIndices[t * 3 + 2] : triIndices[t * 3 + 1];
						uint32_t c2 = flipWinding ? triIndices[t * 3 + 1] : triIndices[t * 3 + 2];

						uint32_t triCorners[3] = { c0, c1, c2 };
						for (uint32_t corner : triCorners)
						{
							if (partVertexMap[corner] != UINT32_MAX)
							{
								data.indices.push_back(partVertexMap[corner]);
							}
							else
							{
								VertexMesh v{};

								ufbx_vec3 pos = mesh->vertex_position[corner];
								ufbx_vec3 worldPos = ufbx_transform_position(&node->geometry_to_world, pos);
								v.position = Math::Vector3(static_cast<float>(worldPos.x), static_cast<float>(worldPos.y), static_cast<float>(worldPos.z));

								subMin = Math::Vector3::Min(subMin, v.position);
								subMax = Math::Vector3::Max(subMax, v.position);

								if (mesh->vertex_normal.exists)
								{
									ufbx_vec3 norm = mesh->vertex_normal[corner];
									ufbx_vec3 worldNorm = ufbx_transform_direction(&normMat, norm);
									Math::Vector3 nVec(static_cast<float>(worldNorm.x), static_cast<float>(worldNorm.y), static_cast<float>(worldNorm.z));
									if (nVec.LengthSquared() > 1e-20f) nVec.Normalize();
									else nVec = Math::Vector3(0.0f, 1.0f, 0.0f);
									v.normal = nVec;
								}
								else
								{
									v.normal = Math::Vector3(0.0f, 1.0f, 0.0f);
								}

								if (mesh->vertex_uv.exists)
								{
									ufbx_vec2 uv = mesh->vertex_uv[corner];
									v.texcoord = Math::Vector2(static_cast<float>(uv.x), 1.0f - static_cast<float>(uv.y));
								}
								else
								{
									v.texcoord = Math::Vector2(0.0f, 0.0f);
								}

								if (mesh->vertex_tangent.exists)
								{
									ufbx_vec3 tan = mesh->vertex_tangent[corner];
									ufbx_vec3 worldTan = ufbx_transform_direction(&node->geometry_to_world, tan);
									Math::Vector3 tanVec(static_cast<float>(worldTan.x), static_cast<float>(worldTan.y), static_cast<float>(worldTan.z));
									if (tanVec.LengthSquared() > 1e-20f) tanVec.Normalize();
									else tanVec = (std::abs(v.normal.y) < 0.99f) ? v.normal.Cross(Math::Vector3(0.0f, 1.0f, 0.0f)) : v.normal.Cross(Math::Vector3(1.0f, 0.0f, 0.0f));

									Math::Vector3 orthoTan = tanVec - v.normal * v.normal.Dot(tanVec);
									if (orthoTan.LengthSquared() > 1e-12f)
									{
										orthoTan.Normalize();
										tanVec = orthoTan;
									}
									else
									{
										tanVec = (std::abs(v.normal.y) < 0.99f) ? v.normal.Cross(Math::Vector3(0.0f, 1.0f, 0.0f)) : v.normal.Cross(Math::Vector3(1.0f, 0.0f, 0.0f));
										tanVec.Normalize();
									}

									float handedness = 1.0f;
									if (mesh->vertex_bitangent.exists)
									{
										ufbx_vec3 bitan = mesh->vertex_bitangent[corner];
										ufbx_vec3 worldBitan = ufbx_transform_direction(&node->geometry_to_world, bitan);
										Math::Vector3 bitanVec(static_cast<float>(worldBitan.x), static_cast<float>(worldBitan.y), static_cast<float>(worldBitan.z));
										if (v.normal.Cross(tanVec).Dot(bitanVec) < 0.0f)
										{
											handedness = -1.0f;
										}
									}
									v.tangent = Math::Vector4(tanVec.x, tanVec.y, tanVec.z, handedness);
								}
								else
								{
									v.tangent = Math::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
								}

								v.color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

								uint32_t newIndex = static_cast<uint32_t>(data.vertices.size());
								data.vertices.push_back(v);
								partVertexMap[corner] = newIndex;
								data.indices.push_back(newIndex);
							}
						}
					}
				}

				sub.indexCount = static_cast<uint32_t>(data.indices.size()) - sub.startIndexLocation;
				if (sub.indexCount == 0) continue;

				sub.localBounds.Center = (subMin + subMax) * 0.5f;
				sub.localBounds.Extents = (subMax - subMin) * 0.5f;

				// Generate missing tangents via Lengyel's accumulation if tangent not in file
				if (!mesh->vertex_tangent.exists && data.vertices.size() > subFirstVert)
				{
					std::vector<Math::Vector3> tan1(data.vertices.size() - subFirstVert, Math::Vector3(0.0f, 0.0f, 0.0f));
					std::vector<Math::Vector3> tan2(data.vertices.size() - subFirstVert, Math::Vector3(0.0f, 0.0f, 0.0f));

					size_t triCount = sub.indexCount / 3;
					for (size_t t = 0; t < triCount; ++t)
					{
						uint32_t i0 = data.indices[sub.startIndexLocation + t * 3 + 0];
						uint32_t i1 = data.indices[sub.startIndexLocation + t * 3 + 1];
						uint32_t i2 = data.indices[sub.startIndexLocation + t * 3 + 2];

						const auto& v0 = data.vertices[i0];
						const auto& v1 = data.vertices[i1];
						const auto& v2 = data.vertices[i2];

						Math::Vector3 e1 = v1.position - v0.position;
						Math::Vector3 e2 = v2.position - v0.position;

						float du1 = v1.texcoord.x - v0.texcoord.x;
						float dv1 = v1.texcoord.y - v0.texcoord.y;
						float du2 = v2.texcoord.x - v0.texcoord.x;
						float dv2 = v2.texcoord.y - v0.texcoord.y;

						float detU = du1 * dv2 - du2 * dv1;
						if (std::abs(detU) > 1e-12f)
						{
							float r = 1.0f / detU;
							Math::Vector3 sdir = (e1 * dv2 - e2 * dv1) * r;
							Math::Vector3 tdir = (e2 * du1 - e1 * du2) * r;

							tan1[i0 - subFirstVert] += sdir;
							tan1[i1 - subFirstVert] += sdir;
							tan1[i2 - subFirstVert] += sdir;

							tan2[i0 - subFirstVert] += tdir;
							tan2[i1 - subFirstVert] += tdir;
							tan2[i2 - subFirstVert] += tdir;
						}
					}

					for (size_t i = subFirstVert; i < data.vertices.size(); ++i)
					{
						auto& vert = data.vertices[i];
						const Math::Vector3& norm = vert.normal;
						const Math::Vector3& t = tan1[i - subFirstVert];
						const Math::Vector3& b = tan2[i - subFirstVert];

						if (t.LengthSquared() < 1e-16f)
						{
							Math::Vector3 arbT = (std::abs(norm.y) < 0.99f) ? norm.Cross(Math::Vector3(0.0f, 1.0f, 0.0f)) : norm.Cross(Math::Vector3(1.0f, 0.0f, 0.0f));
							arbT.Normalize();
							vert.tangent = Math::Vector4(arbT.x, arbT.y, arbT.z, 1.0f);
						}
						else
						{
							Math::Vector3 orthoT = t - norm * norm.Dot(t);
							if (orthoT.LengthSquared() > 1e-16f)
							{
								orthoT.Normalize();
							}
							else
							{
								orthoT = (std::abs(norm.y) < 0.99f) ? norm.Cross(Math::Vector3(0.0f, 1.0f, 0.0f)) : norm.Cross(Math::Vector3(1.0f, 0.0f, 0.0f));
								orthoT.Normalize();
							}
							float handedness = (norm.Cross(orthoT).Dot(b) < 0.0f) ? -1.0f : 1.0f;
							vert.tangent = Math::Vector4(orthoT.x, orthoT.y, orthoT.z, handedness);
						}
					}
				}

				// Resolve PBR textures for this submesh material
				std::string diffuseFilename;
				if (mat)
				{
					if (mat->fbx.diffuse_color.texture && mat->fbx.diffuse_color.texture->filename.length > 0)
					{
						diffuseFilename = std::filesystem::path(std::string(mat->fbx.diffuse_color.texture->filename.data, mat->fbx.diffuse_color.texture->filename.length)).filename().string();
					}
					else if (mat->pbr.base_color.texture && mat->pbr.base_color.texture->filename.length > 0)
					{
						diffuseFilename = std::filesystem::path(std::string(mat->pbr.base_color.texture->filename.data, mat->pbr.base_color.texture->filename.length)).filename().string();
					}
				}

				std::string normalFilename;
				std::string bumpFilename;
				std::string ambientFilename;
				if (mat)
				{
					if (mat->fbx.normal_map.texture && mat->fbx.normal_map.texture->filename.length > 0)
					{
						normalFilename = std::filesystem::path(std::string(mat->fbx.normal_map.texture->filename.data, mat->fbx.normal_map.texture->filename.length)).filename().string();
					}
					if (mat->fbx.bump.texture && mat->fbx.bump.texture->filename.length > 0)
					{
						bumpFilename = std::filesystem::path(std::string(mat->fbx.bump.texture->filename.data, mat->fbx.bump.texture->filename.length)).filename().string();
					}
					if (mat->fbx.ambient_color.texture && mat->fbx.ambient_color.texture->filename.length > 0)
					{
						ambientFilename = std::filesystem::path(std::string(mat->fbx.ambient_color.texture->filename.data, mat->fbx.ambient_color.texture->filename.length)).filename().string();
					}
				}

				std::string stem;
				if (!diffuseFilename.empty())
				{
					stem = DeriveTextureStem(diffuseFilename);
				}
				else if (!matName.empty())
				{
					stem = DeriveTextureStem(matName);
				}

				std::string albedoFull;
				if (!diffuseFilename.empty()) albedoFull = findInLookup(diffuseFilename);
				if (albedoFull.empty() && !stem.empty())
				{
					albedoFull = findInLookup(stem + "_Albedo.png");
					if (albedoFull.empty()) albedoFull = findInLookup(stem + "_diffuse.png");
					if (albedoFull.empty()) albedoFull = findInLookup(stem + "_albedo.png");
					if (albedoFull.empty()) albedoFull = findInLookup(stem + "_Diffuse.png");
				}

				std::string normalFull;
				if (!normalFilename.empty()) normalFull = findInLookup(normalFilename);
				if (normalFull.empty() && !bumpFilename.empty()) normalFull = findInLookup(bumpFilename);
				if (normalFull.empty() && !stem.empty())
				{
					normalFull = findInLookup(stem + "_Normal.png");
					if (normalFull.empty()) normalFull = findInLookup(stem + "_normal.png");
					if (normalFull.empty()) normalFull = findInLookup(stem + "_ddn.png");
				}

				std::string roughnessFull;
				if (!stem.empty())
				{
					roughnessFull = findInLookup(stem + "_Roughness.png");
					if (roughnessFull.empty()) roughnessFull = findInLookup(stem + "_roughness.png");
					if (roughnessFull.empty())
					{
						std::string lowerStem = ToLower(stem);
						if (lowerStem.starts_with("sponza_curtain_"))
						{
							roughnessFull = findInLookup("sponza_curtain_roughness.png");
						}
						else if (lowerStem.starts_with("sponza_fabric_"))
						{
							roughnessFull = findInLookup("sponza_fabric_roughness.png");
						}
					}
				}

				std::string metallicFull;
				if (!ambientFilename.empty() && ToLower(ambientFilename).find("metallic") != std::string::npos)
				{
					metallicFull = findInLookup(ambientFilename);
				}
				if (metallicFull.empty() && !stem.empty())
				{
					metallicFull = findInLookup(stem + "_Metallic.png");
					if (metallicFull.empty()) metallicFull = findInLookup(stem + "_metallic.png");
					if (metallicFull.empty())
					{
						std::string lowerStem = ToLower(stem);
						if (lowerStem.starts_with("sponza_curtain_"))
						{
							metallicFull = findInLookup("sponza_curtain_metallic.png");
						}
						else if (lowerStem.starts_with("sponza_fabric_"))
						{
							metallicFull = findInLookup("sponza_fabric_metallic.png");
						}
					}
				}

				std::string checkStr = ToLower(stem) + " " + ToLower(matName);
				bool isFoliage = (checkStr.find("plant") != std::string::npos ||
				                  checkStr.find("thorn") != std::string::npos ||
				                  checkStr.find("leaf") != std::string::npos ||
				                  checkStr.find("leaves") != std::string::npos ||
				                  checkStr.find("foliage") != std::string::npos ||
				                  checkStr.find("ivy") != std::string::npos ||
				                  checkStr.find("vegetation") != std::string::npos ||
				                  checkStr.find("branch") != std::string::npos ||
				                  checkStr.find("bush") != std::string::npos ||
				                  checkStr.find("grass") != std::string::npos ||
				                  checkStr.find("tree") != std::string::npos);

				bool isCutout = isFoliage ||
				                (checkStr.find("chain") != std::string::npos ||
				                 checkStr.find("cutout") != std::string::npos ||
				                 checkStr.find("alpha") != std::string::npos);

				bool isCloth = (checkStr.find("curtain") != std::string::npos ||
				                checkStr.find("fabric") != std::string::npos ||
				                checkStr.find("cloth") != std::string::npos ||
				                checkStr.find("flag") != std::string::npos ||
				                checkStr.find("banner") != std::string::npos ||
				                checkStr.find("tapestry") != std::string::npos ||
				                checkStr.find("drape") != std::string::npos);

				sub.isAlphaCutout = isCutout;
				std::string matPath = "fbx_mat:" + matName +
					"|albedo=" + albedoFull +
					"|normal=" + normalFull +
					"|roughness=" + roughnessFull +
					"|metallic=" + metallicFull +
					"|cutout=" + (isCutout ? "1" : "0");

				if (isFoliage)
				{
					matPath += "|transmission=1";
				}
				if (isCloth)
				{
					matPath += "|cloth=1";
				}
				sub.defaultMaterialPath = std::move(matPath);

				data.submeshes.push_back(sub);
			}
		}

		if (data.vertices.empty())
		{
			ufbx_free_scene(scene);
			return CreateCubeMesh(1.0f);
		}

		// Automatic Centimeter-to-Meter Normalization
		Math::Vector3 overallMin(1e30f, 1e30f, 1e30f);
		Math::Vector3 overallMax(-1e30f, -1e30f, -1e30f);
		for (const auto& v : data.vertices)
		{
			overallMin = Math::Vector3::Min(overallMin, v.position);
			overallMax = Math::Vector3::Max(overallMax, v.position);
		}

		if ((overallMax - overallMin).Length() > 400.0f)
		{
			overallMin *= 0.01f;
			overallMax *= 0.01f;
			for (auto& v : data.vertices)
			{
				v.position *= 0.01f;
			}
			for (auto& sub : data.submeshes)
			{
				sub.localBounds.Center.x *= 0.01f;
				sub.localBounds.Center.y *= 0.01f;
				sub.localBounds.Center.z *= 0.01f;
				sub.localBounds.Extents.x *= 0.01f;
				sub.localBounds.Extents.y *= 0.01f;
				sub.localBounds.Extents.z *= 0.01f;
			}
		}

		data.localBounds.Center = (overallMin + overallMax) * 0.5f;
		data.localBounds.Extents = (overallMax - overallMin) * 0.5f;

		// Merge static FBX node submeshes that share the same material (reduces Sponza from ~380 submeshes to ~25)
		if (data.submeshes.size() > 1)
		{
			struct MatGroup
			{
				std::string name;
				std::string defaultMaterialPath;
				bool isAlphaCutout = false;
				Math::Vector3 minPt{ 1e30f, 1e30f, 1e30f };
				Math::Vector3 maxPt{ -1e30f, -1e30f, -1e30f };
				std::vector<uint32_t> mergedIndices;
			};

			std::vector<MatGroup> groups;
			std::unordered_map<std::string, size_t> groupMap;

			for (const auto& sub : data.submeshes)
			{
				if (sub.indexCount == 0) continue;
				std::string key = (sub.isAlphaCutout ? "1|" : "0|") + sub.defaultMaterialPath;
				size_t gIdx = 0;
				auto it = groupMap.find(key);
				if (it == groupMap.end())
				{
					gIdx = groups.size();
					groupMap[key] = gIdx;
					MatGroup g{};
					g.name = sub.name;
					g.defaultMaterialPath = sub.defaultMaterialPath;
					g.isAlphaCutout = sub.isAlphaCutout;
					groups.push_back(std::move(g));
				}
				else
				{
					gIdx = it->second;
				}

				auto& g = groups[gIdx];
				Math::Vector3 sMin(
					sub.localBounds.Center.x - sub.localBounds.Extents.x,
					sub.localBounds.Center.y - sub.localBounds.Extents.y,
					sub.localBounds.Center.z - sub.localBounds.Extents.z);
				Math::Vector3 sMax(
					sub.localBounds.Center.x + sub.localBounds.Extents.x,
					sub.localBounds.Center.y + sub.localBounds.Extents.y,
					sub.localBounds.Center.z + sub.localBounds.Extents.z);
				g.minPt = Math::Vector3::Min(g.minPt, sMin);
				g.maxPt = Math::Vector3::Max(g.maxPt, sMax);

				auto startIt = data.indices.begin() + sub.startIndexLocation;
				auto endIt = startIt + sub.indexCount;
				g.mergedIndices.insert(g.mergedIndices.end(), startIt, endIt);
			}

			// Sort opaque solid submeshes first, alpha-cutout submeshes second
			std::stable_sort(groups.begin(), groups.end(), [](const MatGroup& a, const MatGroup& b) {
				return (!a.isAlphaCutout && b.isAlphaCutout);
			});

			std::vector<uint32_t> reorderedIndices;
			reorderedIndices.reserve(data.indices.size());
			std::vector<Submesh> mergedSubmeshes;
			mergedSubmeshes.reserve(groups.size());

			for (auto& g : groups)
			{
				if (g.mergedIndices.empty()) continue;
				Submesh merged{};
				merged.name = g.name;
				merged.defaultMaterialPath = g.defaultMaterialPath;
				merged.isAlphaCutout = g.isAlphaCutout;
				merged.startIndexLocation = static_cast<uint32_t>(reorderedIndices.size());
				merged.indexCount = static_cast<uint32_t>(g.mergedIndices.size());
				merged.baseVertexLocation = 0;
				merged.localBounds.Center = (g.minPt + g.maxPt) * 0.5f;
				merged.localBounds.Extents = (g.maxPt - g.minPt) * 0.5f;

				reorderedIndices.insert(reorderedIndices.end(), g.mergedIndices.begin(), g.mergedIndices.end());
				mergedSubmeshes.push_back(std::move(merged));
			}

			data.indices = std::move(reorderedIndices);
			data.submeshes = std::move(mergedSubmeshes);
		}

		GenerateMeshLODs(data);

		ufbx_free_scene(scene);
		return data;
	}
}
