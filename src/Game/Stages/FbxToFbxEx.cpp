#include "Game/Stages/FbxToFbxEx.h"

#include "Game/Stages/FbxExWriter.h"

#include "Game/Stages/FbxAscii.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>

namespace {

constexpr size_t kVertexFloats = FbxExWriter::kVertexFloats;

struct Matrix
{
	double m[16];
};

Matrix Identity()
{
	Matrix out = {};

	for (int i = 0; i < 4; ++i)
		out.m[i * 4 + i] = 1.0;

	return out;
}

Matrix Multiply(const Matrix& a, const Matrix& b)
{
	Matrix out = {};

	for (int r = 0; r < 4; ++r)
	{
		for (int c = 0; c < 4; ++c)
		{
			double sum = 0.0;

			for (int k = 0; k < 4; ++k)
				sum += a.m[r * 4 + k] * b.m[k * 4 + c];

			out.m[r * 4 + c] = sum;
		}
	}

	return out;
}

Matrix Translation(double x, double y, double z)
{
	Matrix out = Identity();
	out.m[12] = x;
	out.m[13] = y;
	out.m[14] = z;
	return out;
}

Matrix Scaling(double x, double y, double z)
{
	Matrix out = Identity();
	out.m[0] = x;
	out.m[5] = y;
	out.m[10] = z;
	return out;
}

Matrix Rotation(double rx, double ry, double rz, int order = 0)
{
	const double d = 3.14159265358979323846 / 180.0;
	const double sx = sin(rx * d), cx = cos(rx * d);
	const double sy = sin(ry * d), cy = cos(ry * d);
	const double sz = sin(rz * d), cz = cos(rz * d);

	Matrix mx = Identity();
	mx.m[5] = cx; mx.m[6] = sx; mx.m[9] = -sx; mx.m[10] = cx;

	Matrix my = Identity();
	my.m[0] = cy; my.m[2] = -sy; my.m[8] = sy; my.m[10] = cy;

	Matrix mz = Identity();
	mz.m[0] = cz; mz.m[1] = sz; mz.m[4] = -sz; mz.m[5] = cz;

	switch (order)
	{
	case 1:
		return Multiply(Multiply(mx, mz), my);
	case 2:
		return Multiply(Multiply(my, mz), mx);
	case 3:
		return Multiply(Multiply(my, mx), mz);
	case 4:
		return Multiply(Multiply(mz, mx), my);
	case 5:
		return Multiply(Multiply(mz, my), mx);
	default:
		return Multiply(Multiply(mx, my), mz);
	}
}

void Normalize(double* v)
{
	const double length = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

	if (length <= 1e-8)
	{
		v[0] = 0.0;
		v[1] = 1.0;
		v[2] = 0.0;
		return;
	}

	v[0] /= length;
	v[1] /= length;
	v[2] /= length;
}

void Apply(const Matrix& m, double x, double y, double z, double w, double* out)
{
	out[0] = x * m.m[0] + y * m.m[4] + z * m.m[8] + w * m.m[12];
	out[1] = x * m.m[1] + y * m.m[5] + z * m.m[9] + w * m.m[13];
	out[2] = x * m.m[2] + y * m.m[6] + z * m.m[10] + w * m.m[14];
}

struct Submesh
{
	int material;
	std::vector<int> indices;
};

struct NodeOut
{
	int type;
	int child;
	int sibling;
	int blendmode;
	Matrix local;
	Matrix world;
	std::vector<float> vertices;
	std::vector<Submesh> submeshes;
};

struct MaterialOut
{
	std::string filename;
	int textureIndex;
	double value[17];
};

struct MaterialProps
{
	double value[17];
};

MaterialProps DefaultMaterial()
{
	const MaterialProps out = { { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 } };

	return out;
}

const std::vector<double>* PropertyNumbers(const FbxAscii::Tree& tree, int model,
	const char* name)
{
	const int props = tree.Find(model, "Properties60");

	if (props < 0)
		return nullptr;

	std::vector<int> entries;
	tree.All(props, "Property", entries);

	for (int entry : entries)
	{
		if (tree.Prop(entry, 0) != name)
			continue;

		return tree.At(entry).numbers.empty() ? nullptr : &tree.At(entry).numbers;
	}

	return nullptr;
}

bool HasProperty(const FbxAscii::Tree& tree, int model, const char* name)
{
	const int props = tree.Find(model, "Properties60");

	if (props < 0)
		return false;

	std::vector<int> entries;
	tree.All(props, "Property", entries);

	for (int entry : entries)
	{
		if (tree.Prop(entry, 0) == name)
			return true;
	}

	return false;
}

const std::vector<double>* PropertyVector(const FbxAscii::Tree& tree, int model, const char* name)
{
	const std::vector<double>* const values = PropertyNumbers(tree, model, name);

	return values != nullptr && values->size() >= 3 ? values : nullptr;
}

double PropertyScalar(const FbxAscii::Tree& tree, int node, const char* name, double fallback)
{
	const std::vector<double>* const values = PropertyNumbers(tree, node, name);

	return values != nullptr ? (*values)[0] : fallback;
}

void PropertyColour(const FbxAscii::Tree& tree, int node, const char* name, double* out)
{
	const std::vector<double>* const values = PropertyVector(tree, node, name);

	if (values == nullptr || values->size() < 3)
		return;

	for (int i = 0; i < 3; ++i)
		out[i] = (*values)[i];

	out[3] = 1.0;
}

MaterialProps ReadMaterial(const FbxAscii::Tree& tree, int node)
{
	MaterialProps out = DefaultMaterial();

	PropertyColour(tree, node, "DiffuseColor", out.value);
	PropertyColour(tree, node, "AmbientColor", out.value + 4);
	PropertyColour(tree, node, "SpecularColor", out.value + 8);
	PropertyColour(tree, node, "EmissiveColor", out.value + 12);

	out.value[16] = PropertyScalar(tree, node, "Shininess",
		PropertyScalar(tree, node, "ShininessExponent", 0.0));

	return out;
}

Matrix TranslationProp(const FbxAscii::Tree& tree, int model, const char* name, double sign)
{
	const std::vector<double>* v = PropertyVector(tree, model, name);

	if (v == nullptr)
		return Identity();

	return Translation(sign * (*v)[0], sign * (*v)[1], sign * (*v)[2]);
}

Matrix RotationProp(const FbxAscii::Tree& tree, int model, const char* name, int order = 0)
{
	const std::vector<double>* v = PropertyVector(tree, model, name);

	return v == nullptr ? Identity() : Rotation((*v)[0], (*v)[1], (*v)[2], order);
}

int RotationOrderOf(const FbxAscii::Tree& tree, int model)
{
	const int props = tree.Find(model, "Properties60");

	if (props < 0)
		return 0;

	std::vector<int> entries;
	tree.All(props, "Property", entries);

	for (int entry : entries)
	{
		if (tree.Prop(entry, 0) != "RotationOrder")
			continue;

		if (tree.At(entry).numbers.empty())
			return 0;

		const int order = static_cast<int>(tree.At(entry).numbers[0]);
		return order >= 0 && order <= 5 ? order : 0;
	}

	return 0;
}

Matrix ScalingProp(const FbxAscii::Tree& tree, int model, const char* name)
{
	const std::vector<double>* v = PropertyVector(tree, model, name);

	return v == nullptr ? Identity() : Scaling((*v)[0], (*v)[1], (*v)[2]);
}

Matrix TransposeRotation(const Matrix& m)
{
	Matrix out = Identity();

	for (int r = 0; r < 3; ++r)
	{
		for (int c = 0; c < 3; ++c)
			out.m[r * 4 + c] = m.m[c * 4 + r];
	}

	return out;
}

Matrix LocalMatrix(const FbxAscii::Tree& tree, int model, const double* animT,
	const double* animR, const double* animS)
{
	const Matrix t = animT != nullptr ? Translation(animT[0], animT[1], animT[2])
		: TranslationProp(tree, model, "Lcl Translation", 1.0);
	const Matrix roff = TranslationProp(tree, model, "RotationOffset", 1.0);
	const Matrix rp = TranslationProp(tree, model, "RotationPivot", 1.0);
	const Matrix rpInv = TranslationProp(tree, model, "RotationPivot", -1.0);
	const Matrix soff = TranslationProp(tree, model, "ScalingOffset", 1.0);
	const Matrix sp = TranslationProp(tree, model, "ScalingPivot", 1.0);
	const Matrix spInv = TranslationProp(tree, model, "ScalingPivot", -1.0);

	const Matrix rpre = RotationProp(tree, model, "PreRotation");
	const int order = RotationOrderOf(tree, model);
	const Matrix r = animR != nullptr ? Rotation(animR[0], animR[1], animR[2], order)
		: RotationProp(tree, model, "Lcl Rotation", order);
	const Matrix rpostInv = TransposeRotation(RotationProp(tree, model, "PostRotation"));
	const Matrix s = animS != nullptr ? Scaling(animS[0], animS[1], animS[2])
		: ScalingProp(tree, model, "Lcl Scaling");

	Matrix out = spInv;
	out = Multiply(out, s);
	out = Multiply(out, sp);
	out = Multiply(out, soff);
	out = Multiply(out, rpInv);
	out = Multiply(out, rpostInv);
	out = Multiply(out, r);
	out = Multiply(out, rpre);
	out = Multiply(out, rp);
	out = Multiply(out, roff);
	out = Multiply(out, t);

	return out;
}

Matrix GeometricMatrix(const FbxAscii::Tree& tree, int model)
{
	const Matrix s = ScalingProp(tree, model, "GeometricScaling");
	const Matrix r = RotationProp(tree, model, "GeometricRotation");
	const Matrix t = TranslationProp(tree, model, "GeometricTranslation", 1.0);

	return Multiply(Multiply(s, r), t);
}

constexpr double kFbxSecond = 46186158000.0;
constexpr double kDefaultFrameRate = 30.0;
constexpr int kMaxFrames = 3600;

int FindDeep(const FbxAscii::Tree& tree, int parent, const char* name)
{
	const int direct = tree.Find(parent, name);

	if (direct >= 0)
		return direct;

	for (int child : tree.At(parent).children)
	{
		const int found = FindDeep(tree, child, name);

		if (found >= 0)
			return found;
	}

	return -1;
}

double FrameRateOf(const FbxAscii::Tree& tree)
{
	const int settings = FindDeep(tree, tree.Root(), "GlobalSettings");
	const std::vector<double>* const mode = settings < 0 ? nullptr
		: PropertyNumbers(tree, settings, "TimeMode");

	double rate = kDefaultFrameRate;

	if (mode != nullptr && !mode->empty())
	{
		switch (static_cast<int>((*mode)[0]))
		{
		case 1: rate = 120.0; break;
		case 2: rate = 100.0; break;
		case 3: rate = 60.0; break;
		case 4: rate = 50.0; break;
		case 5: rate = 48.0; break;
		case 8: rate = 29.97; break;
		case 9: rate = 29.97; break;
		case 10: rate = 25.0; break;
		case 11: rate = 24.0; break;
		case 12: rate = 1000.0; break;
		case 13: rate = 23.976; break;
		case 14: rate = 47.952; break;
		case 15: rate = 59.94; break;
		default: break;
		}
	}

	return rate < 1.0 ? 1.0 : rate;
}

enum Interpolation
{
	Interpolation_Constant,
	Interpolation_Linear,
	Interpolation_Cubic
};

struct Key
{
	double time;
	double value;
	double right;
	double left;
	int interpolation;

	Key() : time(0.0), value(0.0), right(0.0), left(0.0),
		interpolation(Interpolation_Linear) {}
};

struct Curve
{
	bool present;
	bool keyed;
	double fallback;
	std::vector<Key> keys;

	Curve() : present(false), keyed(false), fallback(0.0) {}
};

struct NodeAnim
{
	Curve translation[3];
	Curve rotation[3];
	Curve scaling[3];
};

double Hermite(const Key& from, const Key& to, double blend, double span)
{
	const double u = blend;
	const double uu = u * u;
	const double uuu = uu * u;

	return (2.0 * uuu - 3.0 * uu + 1.0) * from.value
		+ (uuu - 2.0 * uu + u) * span * from.right
		+ (-2.0 * uuu + 3.0 * uu) * to.value
		+ (uuu - uu) * span * from.left;
}

double Sample(const Curve& curve, double seconds)
{
	if (!curve.keyed || curve.keys.empty())
		return curve.fallback;

	if (seconds <= curve.keys.front().time)
		return curve.keys.front().value;

	if (seconds >= curve.keys.back().time)
		return curve.keys.back().value;

	for (size_t i = 1; i < curve.keys.size(); ++i)
	{
		if (seconds > curve.keys[i].time)
			continue;

		const Key& from = curve.keys[i - 1];
		const Key& to = curve.keys[i];
		const double span = to.time - from.time;

		if (span <= 1e-12)
			return to.value;

		if (from.interpolation == Interpolation_Constant)
			return from.value;

		const double blend = (seconds - from.time) / span;

		if (from.interpolation == Interpolation_Linear)
			return from.value + (to.value - from.value) * blend;

		return Hermite(from, to, blend, span);
	}

	return curve.keys.back().value;
}

bool WalkKeys(const FbxAscii::Node& node, int keys, std::vector<Key>& out)
{
	size_t number = 0;
	size_t letter = 0;

	for (int i = 0; i < keys; ++i)
	{
		if (number + 2 > node.numbers.size())
			return false;

		Key key;
		key.time = node.numbers[number] / kFbxSecond;
		key.value = node.numbers[number + 1];
		number += 2;

		if (letter >= node.props.size() || node.propAt[letter] != number)
			return false;

		const std::string& interpolation = node.props[letter++];

		if (interpolation == "C")
		{
			key.interpolation = Interpolation_Constant;
			out.push_back(key);
			continue;
		}

		if (interpolation == "L")
		{
			key.interpolation = Interpolation_Linear;
			out.push_back(key);
			continue;
		}

		if (interpolation != "U")
			return false;

		if (letter >= node.props.size() || node.propAt[letter] != number)
			return false;

		++letter;

		if (number + 2 > node.numbers.size())
			return false;

		key.interpolation = Interpolation_Cubic;
		key.right = node.numbers[number];
		key.left = node.numbers[number + 1];
		number += 2;

		if (letter >= node.props.size() || node.propAt[letter] != number)
			return false;

		if (node.props[letter++] == "a")
			number += 2;

		out.push_back(key);
	}

	return number == node.numbers.size() && letter == node.props.size();
}

void ReadCurve(const FbxAscii::Tree& tree, int axis, Curve& out)
{
	const int def = tree.Find(axis, "Default");

	if (def >= 0 && !tree.At(def).numbers.empty())
	{
		out.fallback = tree.At(def).numbers[0];
		out.present = true;
	}

	const int count = tree.Find(axis, "KeyCount");
	const int key = tree.Find(axis, "Key");

	if (count < 0 || key < 0 || tree.At(count).numbers.empty())
		return;

	const int keys = static_cast<int>(tree.At(count).numbers[0]);
	const std::vector<double>& raw = tree.At(key).numbers;

	if (keys <= 0 || raw.size() < static_cast<size_t>(keys) * 2)
		return;

	if (!WalkKeys(tree.At(key), keys, out.keys))
	{
		out.keys.clear();

		const size_t stride = raw.size() / static_cast<size_t>(keys);

		if (stride < 2)
			return;

		for (int i = 0; i < keys; ++i)
		{
			const size_t at = static_cast<size_t>(i) * stride;

			Key key;
			key.time = raw[at] / kFbxSecond;
			key.value = raw[at + 1];
			out.keys.push_back(key);
		}
	}

	out.keyed = !out.keys.empty();
	out.present = out.present || out.keyed;
}

int FindChannel(const FbxAscii::Tree& tree, int parent, const char* label)
{
	if (parent < 0)
		return -1;

	std::vector<int> list;
	tree.All(parent, "Channel", list);

	for (int child : list)
	{
		if (tree.Prop(child, 0) == label)
			return child;
	}

	return -1;
}

void ReadTriple(const FbxAscii::Tree& tree, int group, Curve* out)
{
	const char* const axes[] = { "X", "Y", "Z" };

	for (int i = 0; i < 3; ++i)
	{
		const int axis = FindChannel(tree, group, axes[i]);

		if (axis >= 0)
			ReadCurve(tree, axis, out[i]);
	}
}

void ReadTakes(const FbxAscii::Tree& tree, std::map<std::string, NodeAnim>& out,
	double& start, double& end)
{
	const int takes = tree.Find(tree.Root(), "Takes");

	if (takes < 0)
		return;

	std::vector<int> list;
	tree.All(takes, "Take", list);

	if (list.empty())
		return;

	const int take = list.front();
	const int span = tree.Find(take, "LocalTime");

	if (span >= 0 && tree.At(span).numbers.size() >= 2)
	{
		start = tree.At(span).numbers[0] / kFbxSecond;
		end = tree.At(span).numbers[1] / kFbxSecond;
	}

	std::vector<int> models;
	tree.All(take, "Model", models);

	for (int model : models)
	{
		const std::string& name = tree.Prop(model, 0);

		if (name.empty())
			continue;

		const int transform = FindChannel(tree, model, "Transform");

		if (transform < 0)
			continue;

		NodeAnim anim;
		ReadTriple(tree, FindChannel(tree, transform, "T"), anim.translation);
		ReadTriple(tree, FindChannel(tree, transform, "R"), anim.rotation);
		ReadTriple(tree, FindChannel(tree, transform, "S"), anim.scaling);

		bool keyed = false;

		for (int i = 0; i < 3; ++i)
			keyed = keyed || anim.translation[i].keyed || anim.rotation[i].keyed
				|| anim.scaling[i].keyed;

		if (keyed)
			out[name] = anim;
	}
}

Matrix PosedLocal(const FbxAscii::Tree& tree, int model, const NodeAnim& anim, double seconds)
{
	const std::vector<double>* staticT = PropertyVector(tree, model, "Lcl Translation");
	const std::vector<double>* staticR = PropertyVector(tree, model, "Lcl Rotation");
	const std::vector<double>* staticS = PropertyVector(tree, model, "Lcl Scaling");

	double t[3] = { 0.0, 0.0, 0.0 };
	double r[3] = { 0.0, 0.0, 0.0 };
	double s[3] = { 1.0, 1.0, 1.0 };

	for (int i = 0; i < 3; ++i)
	{
		t[i] = anim.translation[i].present ? Sample(anim.translation[i], seconds)
			: (staticT != nullptr ? (*staticT)[i] : 0.0);
		r[i] = anim.rotation[i].present ? Sample(anim.rotation[i], seconds)
			: (staticR != nullptr ? (*staticR)[i] : 0.0);
		s[i] = anim.scaling[i].present ? Sample(anim.scaling[i], seconds)
			: (staticS != nullptr ? (*staticS)[i] : 1.0);
	}

	return LocalMatrix(tree, model, t, r, s);
}

double FrameSeconds(double start, int frame, double rate)
{
	return start + static_cast<double>(frame) / rate;
}

bool KeySpan(const NodeAnim& anim, double& first, double& last)
{
	bool any = false;

	for (int i = 0; i < 3; ++i)
	{
		const Curve* const curves[3] = { &anim.translation[i], &anim.rotation[i],
			&anim.scaling[i] };

		for (const Curve* curve : curves)
		{
			if (!curve->keyed || curve->keys.empty())
				continue;

			const double low = curve->keys.front().time;
			const double high = curve->keys.back().time;

			first = any && first <= low ? first : low;
			last = any && last >= high ? last : high;
			any = true;
		}
	}

	return any;
}

std::string BaseName(const std::string& path)
{
	size_t cut = path.find_last_of("\\/");
	return cut == std::string::npos ? path : path.substr(cut + 1);
}

const std::vector<double>* LayerData(const FbxAscii::Tree& tree, int model, const char* layer,
	const char* key, std::string& mapping, std::string& reference)
{
	const int element = tree.Find(model, layer);

	if (element < 0)
		return nullptr;

	mapping = tree.Prop(tree.Find(element, "MappingInformationType"), 0);
	reference = tree.Prop(tree.Find(element, "ReferenceInformationType"), 0);

	const int data = tree.Find(element, key);

	return data < 0 ? nullptr : &tree.At(data).numbers;
}

}

bool FbxToFbxEx::Convert(const uint8_t* fbx, size_t size, std::vector<uint8_t>& out,
	std::string& error)
{
	FbxAscii::Tree tree;

	if (!tree.Parse(fbx, size))
	{
		error = "that bg.fbx could not be read as FBX text";
		return false;
	}

	const int objects = tree.Find(tree.Root(), "Objects");
	const int connections = tree.Find(tree.Root(), "Connections");

	if (objects < 0)
	{
		error = "that bg.fbx has no Objects section";
		return false;
	}

	std::vector<int> models;
	tree.All(objects, "Model", models);

	std::map<std::string, int> byName;
	std::vector<std::string> order;

	for (int model : models)
	{
		const std::string& name = tree.Prop(model, 0);

		if (name.empty() || byName.count(name) != 0)
			continue;

		byName[name] = model;
		order.push_back(name);
	}

	if (byName.empty())
	{
		error = "that bg.fbx holds no models";
		return false;
	}

	std::vector<int> textures;
	tree.All(objects, "Texture", textures);

	std::map<std::string, std::string> textureFile;

	for (int texture : textures)
	{
		int file = tree.Find(texture, "RelativeFilename");

		if (file < 0)
			file = tree.Find(texture, "FileName");

		if (file < 0)
			continue;

		textureFile[tree.Prop(texture, 0)] = BaseName(tree.Prop(file, 0));
	}

	std::vector<int> surfaces;
	tree.All(objects, "Material", surfaces);

	std::map<std::string, MaterialProps> materialProps;

	for (int surface : surfaces)
		materialProps[tree.Prop(surface, 0)] = ReadMaterial(tree, surface);

	std::map<std::string, std::string> parent;
	std::map<std::string, std::vector<std::string> > meshTextures;
	std::map<std::string, std::vector<std::string> > meshMaterials;

	if (connections >= 0)
	{
		std::vector<int> links;
		tree.All(connections, "Connect", links);

		for (int link : links)
		{
			if (tree.Prop(link, 0) != "OO")
				continue;

			const std::string& src = tree.Prop(link, 1);
			const std::string& dst = tree.Prop(link, 2);

			if (src.compare(0, 7, "Model::") == 0 && dst.compare(0, 7, "Model::") == 0)
			{
				parent[src] = dst;
				continue;
			}

			if (src.compare(0, 10, "Material::") == 0 && dst.compare(0, 7, "Model::") == 0)
			{
				meshMaterials[dst].push_back(src);
				continue;
			}

			if (src.compare(0, 9, "Texture::") == 0 && dst.compare(0, 7, "Model::") == 0)
			{
				std::map<std::string, std::string>::const_iterator found = textureFile.find(src);
				meshTextures[dst].push_back(found == textureFile.end() ? std::string()
					: found->second);
			}
		}
	}

	std::map<std::string, std::vector<std::string> > kids;

	for (const std::string& name : order)
	{
		std::map<std::string, std::string>::const_iterator up = parent.find(name);
		const std::string owner = up == parent.end() ? std::string("Model::Scene") : up->second;
		kids[owner].push_back(name);
	}

	std::vector<std::string> walkOrder;
	std::set<std::string> seen;
	std::vector<std::string> stack;

	for (const std::string& name : order)
	{
		std::map<std::string, std::string>::const_iterator up = parent.find(name);

		if (up != parent.end() && byName.count(up->second) != 0)
			continue;

		stack.push_back(name);

		while (!stack.empty())
		{
			const std::string current = stack.back();
			stack.pop_back();

			if (!seen.insert(current).second)
				continue;

			walkOrder.push_back(current);

			const std::vector<std::string>& list = kids[current];

			for (size_t i = list.size(); i > 0; --i)
				stack.push_back(list[i - 1]);
		}
	}

	for (const std::string& name : order)
	{
		if (seen.insert(name).second)
			walkOrder.push_back(name);
	}

	std::map<std::string, int> slot;

	for (size_t i = 0; i < walkOrder.size(); ++i)
		slot[walkOrder[i]] = static_cast<int>(i);

	std::map<std::string, NodeAnim> anims;
	double takeStart = 0.0;
	double takeEnd = 0.0;
	ReadTakes(tree, anims, takeStart, takeEnd);

	const double rate = FrameRateOf(tree);

	int frames = 1;

	if (!anims.empty() && takeEnd > takeStart)
	{
		frames = static_cast<int>((takeEnd - takeStart) * rate + 0.5);
		frames = frames < 2 ? 1 : (frames > kMaxFrames ? kMaxFrames : frames);
	}

	std::map<std::string, Matrix> world;

	std::vector<NodeOut> nodes(walkOrder.size());
	std::vector<MaterialOut> materials;
	std::vector<std::string> textureNames;
	std::map<std::string, int> textureSlot;

	for (size_t index = 0; index < walkOrder.size(); ++index)
	{
		const std::string& name = walkOrder[index];
		const int model = byName[name];

		NodeOut& node = nodes[index];
		node.type = 0;
		node.child = -1;
		node.sibling = -1;
		node.blendmode = HasProperty(tree, model, "adding") ? 1 : 0;

		const std::vector<std::string>& list = kids[name];
		int first = -1;

		for (const std::string& kid : list)
		{
			std::map<std::string, int>::const_iterator at = slot.find(kid);

			if (at != slot.end() && (first < 0 || at->second < first))
				first = at->second;
		}

		node.child = first;

		const std::map<std::string, NodeAnim>::const_iterator posed = anims.find(name);
		const Matrix local = posed != anims.end() && frames > 1
			? PosedLocal(tree, model, posed->second, takeStart)
			: LocalMatrix(tree, model, nullptr, nullptr, nullptr);
		std::map<std::string, std::string>::const_iterator up = parent.find(name);
		Matrix accumulated = local;

		if (up != parent.end() && world.count(up->second) != 0)
			accumulated = Multiply(local, world[up->second]);

		world[name] = accumulated;
		node.local = local;
		node.world = accumulated;

		const int verticesNode = tree.Find(model, "Vertices");

		if (verticesNode < 0)
			continue;

		const std::vector<double>& positions = tree.At(verticesNode).numbers;
		const int polygonNode = tree.Find(model, "PolygonVertexIndex");

		if (positions.empty() || polygonNode < 0)
			continue;

		const std::vector<double>& polygons = tree.At(polygonNode).numbers;

		std::string normalMapping, normalReference;
		const std::vector<double>* normals = LayerData(tree, model, "LayerElementNormal",
			"Normals", normalMapping, normalReference);

		std::string uvMapping, uvReference;
		const std::vector<double>* uvs = LayerData(tree, model, "LayerElementUV", "UV",
			uvMapping, uvReference);

		const int uvElement = tree.Find(model, "LayerElementUV");
		const int uvIndexNode = uvElement < 0 ? -1 : tree.Find(uvElement, "UVIndex");
		const std::vector<double>* uvIndex = uvIndexNode < 0 ? nullptr
			: &tree.At(uvIndexNode).numbers;

		std::string colourMapping, colourReference;
		const std::vector<double>* colours = LayerData(tree, model, "LayerElementColor",
			"Colors", colourMapping, colourReference);

		const int colourElement = tree.Find(model, "LayerElementColor");
		const int colourIndexNode = colourElement < 0 ? -1
			: tree.Find(colourElement, "ColorIndex");
		const std::vector<double>* colourIndex = colourIndexNode < 0 ? nullptr
			: &tree.At(colourIndexNode).numbers;

		std::string materialMapping, materialReference;
		const std::vector<double>* materialLayer = LayerData(tree, model,
			"LayerElementMaterial", "Materials", materialMapping, materialReference);

		std::string textureMapping, textureReference;
		const std::vector<double>* textureLayer = LayerData(tree, model,
			"LayerElementTexture", "TextureId", textureMapping, textureReference);

		const Matrix geo = GeometricMatrix(tree, model);

		std::map<int, std::vector<int> > byMaterial;
		std::map<int, int> groupSurface;
		std::vector<std::pair<int, int> > face;
		int polygon = 0;

		for (size_t k = 0; k < polygons.size(); ++k)
		{
			const int raw = static_cast<int>(polygons[k]);
			const int vi = raw >= 0 ? raw : -raw - 1;

			face.push_back(std::make_pair(vi, static_cast<int>(k)));

			if (raw >= 0)
				continue;

			int surface = 0;

			if (materialLayer != nullptr && !materialLayer->empty())
			{
				if (materialMapping == "AllSame")
					surface = static_cast<int>((*materialLayer)[0]);
				else if (polygon < static_cast<int>(materialLayer->size()))
					surface = static_cast<int>((*materialLayer)[polygon]);
			}

			int material = surface;

			if (textureLayer != nullptr && !textureLayer->empty())
			{
				if (textureMapping == "AllSame")
					material = static_cast<int>((*textureLayer)[0]);
				else if (polygon < static_cast<int>(textureLayer->size()))
					material = static_cast<int>((*textureLayer)[polygon]);
			}

			if (groupSurface.count(material) == 0)
				groupSurface[material] = surface;

			const int base = static_cast<int>(node.vertices.size() / kVertexFloats);

			for (const std::pair<int, int>& corner : face)
			{
				const size_t at = static_cast<size_t>(corner.first) * 3;

				double position[3] = { 0.0, 0.0, 0.0 };

				if (at + 2 < positions.size())
					Apply(geo, positions[at], positions[at + 1], positions[at + 2], 1.0, position);

				double normal[3] = { 0.0, 1.0, 0.0 };

				if (normals != nullptr && !normals->empty())
				{
					const size_t ni = static_cast<size_t>(
						normalMapping == "ByVertice" ? corner.first : corner.second) * 3;

					if (ni + 2 < normals->size())
						Apply(geo, (*normals)[ni], (*normals)[ni + 1], (*normals)[ni + 2], 0.0,
							normal);
				}

				Normalize(normal);

				double u = 0.0;
				double v = 0.0;

				if (uvs != nullptr && !uvs->empty())
				{
					size_t ui = static_cast<size_t>(
						uvMapping == "ByVertice" ? corner.first : corner.second);

					if (uvReference == "IndexToDirect" && uvIndex != nullptr
						&& corner.second < static_cast<int>(uvIndex->size()))
						ui = static_cast<size_t>((*uvIndex)[corner.second]);

					if (ui * 2 + 1 < uvs->size())
					{
						u = (*uvs)[ui * 2];
						v = (*uvs)[ui * 2 + 1];
					}
				}

				double tint[4] = { 1.0, 1.0, 1.0, 1.0 };

				if (colours != nullptr && !colours->empty())
				{
					size_t ci = static_cast<size_t>(
						colourMapping == "ByVertice" ? corner.first : corner.second);

					if (colourReference == "IndexToDirect" && colourIndex != nullptr
						&& corner.second < static_cast<int>(colourIndex->size()))
						ci = static_cast<size_t>((*colourIndex)[corner.second]);

					if (ci * 4 + 3 < colours->size())
					{
						for (int i = 0; i < 4; ++i)
							tint[i] = (*colours)[ci * 4 + i];
					}
				}

				const double values[kVertexFloats] = {
					position[0], position[1], position[2],
					normal[0], normal[1], normal[2],
					tint[0], tint[1], tint[2], tint[3],
					u, v,
				};

				for (double value : values)
					node.vertices.push_back(static_cast<float>(value));
			}

			std::vector<int>& list = byMaterial[material];

			for (size_t j = 1; j + 1 < face.size(); ++j)
			{
				list.push_back(base);
				list.push_back(base + static_cast<int>(j));
				list.push_back(base + static_cast<int>(j) + 1);
			}

			face.clear();
			++polygon;
		}

		const std::vector<std::string>& names = meshTextures[name];
		const std::vector<std::string>& surfaceNames = meshMaterials[name];

		for (std::map<int, std::vector<int> >::iterator it = byMaterial.begin();
			it != byMaterial.end(); ++it)
		{
			std::string filename;

			if (it->first >= 0 && it->first < static_cast<int>(names.size()))
				filename = names[it->first];
			else if (!names.empty())
				filename = names[0];

			std::map<std::string, int>::const_iterator known = textureSlot.find(filename);

			if (known == textureSlot.end())
			{
				textureSlot[filename] = static_cast<int>(textureNames.size());
				textureNames.push_back(filename);
			}

			const int surface = groupSurface.count(it->first) != 0 ? groupSurface[it->first] : 0;
			MaterialProps props = DefaultMaterial();

			if (surface >= 0 && surface < static_cast<int>(surfaceNames.size()))
			{
				const std::map<std::string, MaterialProps>::const_iterator described =
					materialProps.find(surfaceNames[surface]);

				if (described != materialProps.end())
					props = described->second;
			}

			MaterialOut material;
			material.filename = filename;
			material.textureIndex = textureSlot[filename];
			memcpy(material.value, props.value, sizeof(material.value));
			materials.push_back(material);

			Submesh submesh;
			submesh.material = static_cast<int>(materials.size()) - 1;
			submesh.indices.swap(it->second);
			node.submeshes.push_back(submesh);
		}

		node.type = 1;
	}

	for (const std::string& name : walkOrder)
	{
		std::vector<int> sorted;

		for (const std::string& kid : kids[name])
		{
			std::map<std::string, int>::const_iterator at = slot.find(kid);

			if (at != slot.end())
				sorted.push_back(at->second);
		}

		std::sort(sorted.begin(), sorted.end());

		for (size_t i = 0; i + 1 < sorted.size(); ++i)
			nodes[sorted[i]].sibling = sorted[i + 1];
	}

	std::vector<int> roots;

	for (const std::string& name : walkOrder)
	{
		std::map<std::string, std::string>::const_iterator up = parent.find(name);

		if (up == parent.end() || byName.count(up->second) == 0)
			roots.push_back(slot[name]);
	}

	std::sort(roots.begin(), roots.end());

	for (size_t i = 0; i + 1 < roots.size(); ++i)
		nodes[roots[i]].sibling = roots[i + 1];

	if (textureNames.empty())
		textureNames.push_back(std::string());

	FbxExWriter::Model built;
	built.textures = textureNames;

	for (size_t i = 0; i < materials.size(); ++i)
	{
		FbxExWriter::Material material = {};
		material.filename = materials[i].filename;
		material.textureIndex = materials[i].textureIndex;

		for (int k = 0; k < FbxExWriter::kMaterialValues; ++k)
			material.value[k] = static_cast<float>(materials[i].value[k]);

		built.materials.push_back(material);
	}

	FbxExWriter::Node root = FbxExWriter::Branch();
	root.child = nodes.empty() ? -1 : 1;

	built.nodes.push_back(root);

	std::vector<float> rest;

	for (int i = 0; i < FbxExWriter::kMatrixFloats; ++i)
		rest.push_back((i % 5) == 0 ? 1.0f : 0.0f);

	built.animes.push_back(rest);

	for (const NodeOut& node : nodes)
	{
		FbxExWriter::Node written = FbxExWriter::Branch();
		written.type = node.type;
		written.child = node.child >= 0 ? node.child + 1 : -1;
		written.sibling = node.sibling >= 0 ? node.sibling + 1 : -1;
		written.blendmode = node.blendmode;
		written.vertices = node.vertices;

		for (int i = 0; i < FbxExWriter::kMatrixFloats; ++i)
			written.matrix[i] = static_cast<float>(node.world.m[i]);

		for (const Submesh& submesh : node.submeshes)
		{
			FbxExWriter::Submesh part;
			part.material = submesh.material;
			part.indices = submesh.indices;

			written.submeshes.push_back(part);
		}

		built.nodes.push_back(written);
	}

	for (size_t slotIndex = 0; slotIndex < walkOrder.size(); ++slotIndex)
	{
		const std::string& name = walkOrder[slotIndex];
		const std::map<std::string, NodeAnim>::const_iterator found = anims.find(name);

		double first = 0.0;
		double last = 0.0;
		int nodeFrames = 0;

		if (found != anims.end() && frames > 1 && KeySpan(found->second, first, last))
		{
			nodeFrames = static_cast<int>((last - first) * rate + 0.5);
			nodeFrames = nodeFrames > kMaxFrames ? kMaxFrames : nodeFrames;
		}

		std::vector<float> track;

		if (nodeFrames < 2)
		{
			for (int i = 0; i < FbxExWriter::kMatrixFloats; ++i)
				track.push_back(static_cast<float>(nodes[slotIndex].local.m[i]));

			built.animes.push_back(track);
			continue;
		}

		const int model = byName[name];

		for (int frame = 0; frame < nodeFrames; ++frame)
		{
			const Matrix posed = PosedLocal(tree, model, found->second,
				FrameSeconds(first, frame, rate));

			for (int i = 0; i < FbxExWriter::kMatrixFloats; ++i)
				track.push_back(static_cast<float>(posed.m[i]));
		}

		built.animes.push_back(track);
	}

	FbxExWriter::Build(built, out);

	return true;
}
