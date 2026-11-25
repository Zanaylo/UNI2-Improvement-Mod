#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <Windows.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

class RmlRenderGDI final : public Rml::RenderInterface
{
public:
	RmlRenderGDI();
	~RmlRenderGDI() override;

	void Resize(int width, int height);
	void BeginFrame(uint32_t clearColorBgra);
	void Present(HDC targetDc);

	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
		Rml::Span<const int> indices) override;
	void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
		Rml::TextureHandle texture) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

	Rml::TextureHandle LoadTexture(Rml::Vector2i& textureDimensions, const Rml::String& source) override;
	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
		Rml::Vector2i sourceDimensions) override;
	void ReleaseTexture(Rml::TextureHandle texture) override;

	void EnableScissorRegion(bool enable) override;
	void SetScissorRegion(Rml::Rectanglei region) override;

	void EnableClipMask(bool enable) override;
	void RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry,
		Rml::Vector2f translation) override;
	void SetTransform(const Rml::Matrix4f* transform) override;

private:
	struct SoftVertex
	{
		float x;
		float y;
		uint8_t b;
		uint8_t g;
		uint8_t r;
		uint8_t a;
		float u;
		float v;
	};

	struct Geometry
	{
		std::vector<SoftVertex> vertices;
		std::vector<int> indices;
	};

	struct Texture
	{
		int width = 0;
		int height = 0;
		std::vector<uint8_t> bgra;
	};

	void DestroySurface();
	void TransformPoint(float x, float y, float tx, float ty, float& outX, float& outY) const;
	void DrawSolidAxisQuad(int x0, int y0, int x1, int y1, uint8_t b, uint8_t g, uint8_t r, uint8_t a);
	void DrawTexturedAxisQuad(float x0, float y0, float x1, float y1,
		float u0, float v0, float u1, float v1,
		uint8_t mb, uint8_t mg, uint8_t mr, uint8_t ma, const Texture& tex);
	void RasterizeTriangle(const SoftVertex& v0, const SoftVertex& v1, const SoftVertex& v2,
		const Texture* tex, bool writeStencil, uint8_t stencilOp);

	HDC m_memDc = nullptr;
	HBITMAP m_dib = nullptr;
	HBITMAP m_oldBmp = nullptr;
	uint32_t* m_pixels = nullptr;
	int m_width = 0;
	int m_height = 0;

	bool m_scissorEnabled = false;
	int m_scissorL = 0;
	int m_scissorT = 0;
	int m_scissorR = 0;
	int m_scissorB = 0;

	bool m_clipMaskEnabled = false;
	uint8_t m_stencilRef = 1;
	std::vector<uint8_t> m_stencil;

	bool m_hasTransform = false;
	float m_mat[16] = {};

	uintptr_t m_nextHandle = 1;
	std::unordered_map<uintptr_t, Geometry> m_geometries;
	std::unordered_map<uintptr_t, Texture> m_textures;
};
