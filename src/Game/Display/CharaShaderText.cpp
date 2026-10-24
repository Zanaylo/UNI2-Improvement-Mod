#include "Game/Display/CharaShaderText.h"

#include <regex>

namespace {

constexpr char kAnchor[] = "float g_GrayScale;";
constexpr char kSignatureA[] = "TexSample_Palette";
constexpr char kSignatureB[] = "CharaDrawBgColorBase";
constexpr char kPixelCoordinate[] = "In.texuv";
constexpr char kBaseCoordinate[] = "base_pos";

constexpr char kHelpers[] = R"(

float4 ImTexel : register( c180 );

float4 ImPaletteTexel( float2 uv )
{
	float4 index_pal = tex2Dlod( TexSample_Empty, float4( uv, 0.0f, 0.0f ) );
	if( index_pal.a == 0.0f )  return float4( 0.0f, 0.0f, 0.0f, 0.0f );
	int iPal = round( index_pal.a * 255.0f );
	return tex2Dlod( TexSample_Palette, float4( iPal * (1.0f/256.0f), fPalPlusV, 0.0f, 0.0f ) );
}

float ImMagnification( float2 uv )
{
	float2 perPixel = abs( ddx( uv ) ) + abs( ddy( uv ) );
	return min( ImTexel.x / max( perPixel.x, 1e-7f ), ImTexel.y / max( perPixel.y, 1e-7f ) );
}

float4 ImPremultiplied( float2 origin, float2 offset )
{
	float4 texel = ImPaletteTexel( ( origin + offset + 0.5f ) * ImTexel.xy );
	texel.rgb *= texel.a;
	return texel;
}

float ImEdge( float4 texel )
{
	return texel.g * 0.5f + texel.a * 0.5f;
}

void ImAnalyse( inout float2 dir, inout float len, float weight, float a, float b, float c, float d, float e )
{
	float horizontal = d - b;
	float lenX = saturate( abs( horizontal ) / max( max( abs( d - c ), abs( c - b ) ), 0.000001f ) );
	dir.x += horizontal * weight;
	len += lenX * lenX * weight;

	float vertical = e - a;
	float lenY = saturate( abs( vertical ) / max( max( abs( e - c ), abs( c - a ) ), 0.000001f ) );
	dir.y += vertical * weight;
	len += lenY * lenY * weight;
}

void ImAccumulate( inout float4 colour, inout float weight, float2 offset, float2 dir, float2 len, float lob,
	float clip, float4 tap )
{
	float2 rotated = float2( offset.x * dir.x + offset.y * dir.y, offset.x * -dir.y + offset.y * dir.x ) * len;
	float distance = min( dot( rotated, rotated ), clip );
	float base = 0.4f * distance - 1.0f;
	float window = lob * distance - 1.0f;
	base *= base;
	window *= window;
	float amount = ( 1.5625f * base - 0.5625f ) * window;
	colour += tap * amount;
	weight += amount;
}

float4 ImPaletteSmooth( float2 uv )
{
	if( ImTexel.x == 0.0f || ImMagnification( uv ) < 1.25f )  return ImPaletteTexel( uv );

	float2 position = uv * ImTexel.zw - 0.5f;
	float2 origin = floor( position );
	float2 fraction = position - origin;

	float4 b = ImPremultiplied( origin, float2( 0.0f, -1.0f ) );
	float4 c = ImPremultiplied( origin, float2( 1.0f, -1.0f ) );
	float4 e = ImPremultiplied( origin, float2( -1.0f, 0.0f ) );
	float4 f = ImPremultiplied( origin, float2( 0.0f, 0.0f ) );
	float4 g = ImPremultiplied( origin, float2( 1.0f, 0.0f ) );
	float4 h = ImPremultiplied( origin, float2( 2.0f, 0.0f ) );
	float4 i = ImPremultiplied( origin, float2( -1.0f, 1.0f ) );
	float4 j = ImPremultiplied( origin, float2( 0.0f, 1.0f ) );
	float4 k = ImPremultiplied( origin, float2( 1.0f, 1.0f ) );
	float4 l = ImPremultiplied( origin, float2( 2.0f, 1.0f ) );
	float4 n = ImPremultiplied( origin, float2( 0.0f, 2.0f ) );
	float4 o = ImPremultiplied( origin, float2( 1.0f, 2.0f ) );

	float2 dir = float2( 0.0f, 0.0f );
	float len = 0.0f;

	ImAnalyse( dir, len, ( 1.0f - fraction.x ) * ( 1.0f - fraction.y ), ImEdge( b ), ImEdge( e ), ImEdge( f ), ImEdge( g ), ImEdge( j ) );
	ImAnalyse( dir, len, fraction.x * ( 1.0f - fraction.y ), ImEdge( c ), ImEdge( f ), ImEdge( g ), ImEdge( h ), ImEdge( k ) );
	ImAnalyse( dir, len, ( 1.0f - fraction.x ) * fraction.y, ImEdge( f ), ImEdge( i ), ImEdge( j ), ImEdge( k ), ImEdge( n ) );
	ImAnalyse( dir, len, fraction.x * fraction.y, ImEdge( g ), ImEdge( j ), ImEdge( k ), ImEdge( l ), ImEdge( o ) );

	float square = dot( dir, dir );
	bool flat = square < ( 1.0f / 32768.0f );
	dir.x = flat ? 1.0f : dir.x;
	dir *= flat ? 1.0f : rsqrt( max( square, 0.000001f ) );

	len = len * 0.5f;
	len *= len;

	float stretch = 1.0f / max( max( abs( dir.x ), abs( dir.y ) ), 0.000001f );
	float2 extent = float2( 1.0f + ( stretch - 1.0f ) * len, 1.0f - 0.5f * len );
	float lob = 0.5f - 0.29f * len;
	float clip = 1.0f / lob;

	float4 lowest = min( min( f, g ), min( j, k ) );
	float4 highest = max( max( f, g ), max( j, k ) );

	float4 colour = float4( 0.0f, 0.0f, 0.0f, 0.0f );
	float weight = 0.0f;

	ImAccumulate( colour, weight, float2( 0.0f, -1.0f ) - fraction, dir, extent, lob, clip, b );
	ImAccumulate( colour, weight, float2( 1.0f, -1.0f ) - fraction, dir, extent, lob, clip, c );
	ImAccumulate( colour, weight, float2( -1.0f, 0.0f ) - fraction, dir, extent, lob, clip, e );
	ImAccumulate( colour, weight, float2( 0.0f, 0.0f ) - fraction, dir, extent, lob, clip, f );
	ImAccumulate( colour, weight, float2( 1.0f, 0.0f ) - fraction, dir, extent, lob, clip, g );
	ImAccumulate( colour, weight, float2( 2.0f, 0.0f ) - fraction, dir, extent, lob, clip, h );
	ImAccumulate( colour, weight, float2( -1.0f, 1.0f ) - fraction, dir, extent, lob, clip, i );
	ImAccumulate( colour, weight, float2( 0.0f, 1.0f ) - fraction, dir, extent, lob, clip, j );
	ImAccumulate( colour, weight, float2( 1.0f, 1.0f ) - fraction, dir, extent, lob, clip, k );
	ImAccumulate( colour, weight, float2( 2.0f, 1.0f ) - fraction, dir, extent, lob, clip, l );
	ImAccumulate( colour, weight, float2( 0.0f, 2.0f ) - fraction, dir, extent, lob, clip, n );
	ImAccumulate( colour, weight, float2( 1.0f, 2.0f ) - fraction, dir, extent, lob, clip, o );

	float4 pixel = min( highest, max( lowest, colour / weight ) );
	if( pixel.a <= 0.0f )  return float4( 0.0f, 0.0f, 0.0f, 0.0f );

	pixel.rgb /= pixel.a;
	return pixel;
}

bool ImNothingNear( float2 uv )
{
	if( ImTexel.x == 0.0f || ImMagnification( uv ) < 1.25f )
		return tex2Dlod( TexSample_Empty, float4( uv, 0.0f, 0.0f ) ).a == 0.0f;

	float2 origin = floor( uv * ImTexel.zw - 0.5f );
	float coverage = ImPremultiplied( origin, float2( 0.0f, 0.0f ) ).a + ImPremultiplied( origin, float2( 1.0f, 0.0f ) ).a +
		ImPremultiplied( origin, float2( 0.0f, 1.0f ) ).a + ImPremultiplied( origin, float2( 1.0f, 1.0f ) ).a;
	return coverage == 0.0f;
}
)";

const std::regex kStep(
	R"(tex2D\(\s*TexSample_Empty\s*,\s*([A-Za-z_.]+)\s*\))"
	R"(|tex2D\(\s*TexSample_Palette\s*,\s*float2\(\s*fPalU\s*,\s*fPalPlusV\s*\)\s*\))"
	R"(|if\(\s*index_pal\.a\s*==\s*0\.0f\s*\)\s*return 0\.0f;)");

constexpr int kIndexCoordinate = 1;

bool IsCharaShader(const std::string& source)
{
	return source.find(kSignatureA) != std::string::npos && source.find(kSignatureB) != std::string::npos &&
		source.find(kAnchor) != std::string::npos;
}

bool IsDirect(const std::string& coordinate)
{
	return coordinate == kPixelCoordinate || coordinate == kBaseCoordinate;
}

std::string Replacement(const std::smatch& match, const std::string& coordinate, int& replaced)
{
	const std::string found = match.str();

	if (match[kIndexCoordinate].matched || !IsDirect(coordinate))
		return found;

	++replaced;

	if (found.compare(0, 2, "if") == 0)
		return "if( ImNothingNear( " + coordinate + " ) )  return 0.0f;";

	return "ImPaletteSmooth( " + coordinate + " )";
}

std::string SmoothLookups(const std::string& body, int& replaced)
{
	std::string out;
	std::string coordinate;
	auto from = body.cbegin();
	std::smatch match;

	while (std::regex_search(from, body.cend(), match, kStep))
	{
		out.append(from, match[0].first);

		if (match[kIndexCoordinate].matched)
			coordinate = match[kIndexCoordinate].str();

		out += Replacement(match, coordinate, replaced);
		from = match[0].second;
	}

	out.append(from, body.cend());
	return out;
}

}

bool CharaShaderText::Rewrite(const std::string& source, std::string& out, int& replaced)
{
	replaced = 0;

	if (!IsCharaShader(source))
		return false;

	const size_t anchor = source.find(kAnchor) + sizeof(kAnchor) - 1;
	const std::string head = source.substr(0, anchor) + kHelpers;

	out = head + SmoothLookups(source.substr(anchor), replaced);
	return replaced > 0;
}
