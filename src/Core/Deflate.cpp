#include "Core/Deflate.h"

#include <cstring>

namespace {

constexpr int kMaxBits = 15;
constexpr int kLiteralSymbols = 288;
constexpr int kDistanceSymbols = 30;

const uint16_t kLengthBase[29] = {
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
	67, 83, 99, 115, 131, 163, 195, 227, 258
};

const uint8_t kLengthExtra[29] = {
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
	4, 4, 4, 4, 5, 5, 5, 5, 0
};

const uint16_t kDistanceBase[30] = {
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
	1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

const uint8_t kDistanceExtra[30] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
	9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

const uint8_t kCodeLengthOrder[19] = {
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

struct Huffman
{
	uint16_t count[kMaxBits + 1];
	uint16_t symbol[kLiteralSymbols];
};

bool Build(Huffman& table, const uint8_t* lengths, int count)
{
	memset(table.count, 0, sizeof(table.count));

	for (int i = 0; i < count; ++i)
		++table.count[lengths[i]];

	if (table.count[0] == count)
		return false;

	int left = 1;
	for (int bits = 1; bits <= kMaxBits; ++bits)
	{
		left <<= 1;
		left -= table.count[bits];
		if (left < 0)
			return false;
	}

	uint16_t offsets[kMaxBits + 1] = {};
	for (int bits = 1; bits < kMaxBits; ++bits)
		offsets[bits + 1] = static_cast<uint16_t>(offsets[bits] + table.count[bits]);

	for (int i = 0; i < count; ++i)
	{
		if (lengths[i] != 0)
			table.symbol[offsets[lengths[i]]++] = static_cast<uint16_t>(i);
	}

	return true;
}

struct Reader
{
	const uint8_t* source;
	size_t size;
	size_t at;
	uint32_t bitBuffer;
	int bitCount;
	bool failed;

	int Bits(int need)
	{
		uint32_t value = bitBuffer;

		while (bitCount < need)
		{
			if (at >= size)
			{
				failed = true;
				return 0;
			}

			value |= static_cast<uint32_t>(source[at++]) << bitCount;
			bitCount += 8;
		}

		bitBuffer = value >> need;
		bitCount -= need;

		return static_cast<int>(value & ((1u << need) - 1));
	}

	int Decode(const Huffman& table)
	{
		int code = 0;
		int first = 0;
		int index = 0;

		for (int length = 1; length <= kMaxBits; ++length)
		{
			code |= Bits(1);

			if (failed)
				return -1;

			const int count = table.count[length];
			if (code - count < first)
				return table.symbol[index + (code - first)];

			index += count;
			first += count;
			first <<= 1;
			code <<= 1;
		}

		failed = true;
		return -1;
	}
};

bool Stored(Reader& reader, std::vector<uint8_t>& out)
{
	reader.bitBuffer = 0;
	reader.bitCount = 0;

	if (reader.at + 4 > reader.size)
		return false;

	const unsigned length = static_cast<unsigned>(reader.source[reader.at]) |
		(static_cast<unsigned>(reader.source[reader.at + 1]) << 8);
	const unsigned inverse = static_cast<unsigned>(reader.source[reader.at + 2]) |
		(static_cast<unsigned>(reader.source[reader.at + 3]) << 8);

	if ((length ^ 0xffffu) != inverse)
		return false;

	reader.at += 4;

	if (reader.at + length > reader.size)
		return false;

	out.insert(out.end(), reader.source + reader.at, reader.source + reader.at + length);
	reader.at += length;

	return true;
}

bool Block(Reader& reader, const Huffman& literals, const Huffman& distances,
	std::vector<uint8_t>& out)
{
	for (;;)
	{
		const int symbol = reader.Decode(literals);
		if (symbol < 0)
			return false;

		if (symbol < 256)
		{
			out.push_back(static_cast<uint8_t>(symbol));
			continue;
		}

		if (symbol == 256)
			return true;

		const int index = symbol - 257;
		if (index >= 29)
			return false;

		const int length = kLengthBase[index] + reader.Bits(kLengthExtra[index]);

		const int distanceSymbol = reader.Decode(distances);
		if (distanceSymbol < 0 || distanceSymbol >= kDistanceSymbols)
			return false;

		const int distance = kDistanceBase[distanceSymbol] +
			reader.Bits(kDistanceExtra[distanceSymbol]);

		if (reader.failed || distance <= 0 || static_cast<size_t>(distance) > out.size())
			return false;

		size_t from = out.size() - static_cast<size_t>(distance);
		for (int i = 0; i < length; ++i)
			out.push_back(out[from++]);
	}
}

void BuildFixed(Huffman& literals, Huffman& distances)
{
	uint8_t lengths[kLiteralSymbols] = {};

	int i = 0;
	for (; i < 144; ++i) lengths[i] = 8;
	for (; i < 256; ++i) lengths[i] = 9;
	for (; i < 280; ++i) lengths[i] = 7;
	for (; i < 288; ++i) lengths[i] = 8;

	Build(literals, lengths, kLiteralSymbols);

	for (i = 0; i < kDistanceSymbols; ++i)
		lengths[i] = 5;

	Build(distances, lengths, kDistanceSymbols);
}

bool Dynamic(Reader& reader, std::vector<uint8_t>& out)
{
	const int literalCount = reader.Bits(5) + 257;
	const int distanceCount = reader.Bits(5) + 1;
	const int codeCount = reader.Bits(4) + 4;

	if (reader.failed || literalCount > 286 || distanceCount > 30)
		return false;

	uint8_t lengths[kLiteralSymbols + kDistanceSymbols] = {};

	for (int i = 0; i < codeCount; ++i)
		lengths[kCodeLengthOrder[i]] = static_cast<uint8_t>(reader.Bits(3));

	for (int i = codeCount; i < 19; ++i)
		lengths[kCodeLengthOrder[i]] = 0;

	Huffman codeLengths;
	if (reader.failed || !Build(codeLengths, lengths, 19))
		return false;

	const int total = literalCount + distanceCount;
	int index = 0;

	while (index < total)
	{
		const int symbol = reader.Decode(codeLengths);
		if (symbol < 0)
			return false;

		if (symbol < 16)
		{
			lengths[index++] = static_cast<uint8_t>(symbol);
			continue;
		}

		int repeat = 0;
		uint8_t value = 0;

		if (symbol == 16)
		{
			if (index == 0)
				return false;

			value = lengths[index - 1];
			repeat = 3 + reader.Bits(2);
		}
		else if (symbol == 17)
		{
			repeat = 3 + reader.Bits(3);
		}
		else
		{
			repeat = 11 + reader.Bits(7);
		}

		if (reader.failed || index + repeat > total)
			return false;

		while (repeat-- > 0)
			lengths[index++] = value;
	}

	if (lengths[256] == 0)
		return false;

	Huffman literals;
	Huffman distances;

	if (!Build(literals, lengths, literalCount))
		return false;

	Build(distances, lengths + literalCount, distanceCount);

	return Block(reader, literals, distances, out);
}

struct Writer
{
	std::vector<uint8_t>* out;
	uint32_t bitBuffer;
	int bitCount;

	void Bits(uint32_t value, int count)
	{
		bitBuffer |= (value & ((1u << count) - 1)) << bitCount;
		bitCount += count;

		while (bitCount >= 8)
		{
			out->push_back(static_cast<uint8_t>(bitBuffer & 0xff));
			bitBuffer >>= 8;
			bitCount -= 8;
		}
	}

	void Code(uint32_t code, int count)
	{
		uint32_t reversed = 0;

		for (int i = 0; i < count; ++i)
		{
			reversed = (reversed << 1) | (code & 1u);
			code >>= 1;
		}

		Bits(reversed, count);
	}

	void Flush()
	{
		if (bitCount > 0)
			out->push_back(static_cast<uint8_t>(bitBuffer & 0xff));

		bitBuffer = 0;
		bitCount = 0;
	}
};

void WriteLiteral(Writer& writer, int symbol)
{
	if (symbol < 144)
		writer.Code(static_cast<uint32_t>(0x30 + symbol), 8);
	else
		writer.Code(static_cast<uint32_t>(0x190 + symbol - 144), 9);
}

void WriteSymbol(Writer& writer, int symbol)
{
	if (symbol < 256)
	{
		WriteLiteral(writer, symbol);
	}
	else if (symbol < 280)
	{
		writer.Code(static_cast<uint32_t>(symbol - 256), 7);
	}
	else
	{
		writer.Code(static_cast<uint32_t>(0xc0 + symbol - 280), 8);
	}
}

int LengthSymbol(int length)
{
	for (int i = 28; i >= 0; --i)
	{
		if (length >= kLengthBase[i])
			return i;
	}

	return 0;
}

int DistanceSymbol(int distance)
{
	for (int i = 29; i >= 0; --i)
	{
		if (distance >= kDistanceBase[i])
			return i;
	}

	return 0;
}

constexpr int kWindow = 32768;
constexpr int kMinMatch = 3;
constexpr int kMaxMatch = 258;
constexpr int kHashBits = 15;
constexpr int kHashSize = 1 << kHashBits;
constexpr int kMaxChain = 128;

int Hash(const uint8_t* at)
{
	return ((at[0] << 10) ^ (at[1] << 5) ^ at[2]) & (kHashSize - 1);
}

}

bool Deflate::Inflate(const uint8_t* source, size_t size, std::vector<uint8_t>& out, size_t expected)
{
	if (source == nullptr)
		return false;

	out.clear();
	if (expected != 0)
		out.reserve(expected);

	Reader reader = { source, size, 0, 0, 0, false };

	Huffman fixedLiterals;
	Huffman fixedDistances;
	BuildFixed(fixedLiterals, fixedDistances);

	for (;;)
	{
		const int final = reader.Bits(1);
		const int type = reader.Bits(2);

		if (reader.failed)
			return false;

		bool ok = false;

		if (type == 0)
			ok = Stored(reader, out);
		else if (type == 1)
			ok = Block(reader, fixedLiterals, fixedDistances, out);
		else if (type == 2)
			ok = Dynamic(reader, out);

		if (!ok)
			return false;

		if (final != 0)
			return true;
	}
}

bool Deflate::Gunzip(const uint8_t* source, size_t size, std::vector<uint8_t>& out, size_t expected)
{
	if (source == nullptr || size < 18 || source[0] != 0x1f || source[1] != 0x8b || source[2] != 8)
		return false;

	const uint8_t flags = source[3];
	size_t at = 10;

	if (flags & 0x04)
	{
		if (at + 2 > size)
			return false;

		const size_t extra = static_cast<size_t>(source[at]) |
			(static_cast<size_t>(source[at + 1]) << 8);

		at += 2 + extra;
	}

	for (int field = 0; field < 2; ++field)
	{
		if ((flags & (field == 0 ? 0x08 : 0x10)) == 0)
			continue;

		while (at < size && source[at] != 0)
			++at;

		++at;
	}

	if (flags & 0x02)
		at += 2;

	if (at >= size)
		return false;

	return Inflate(source + at, size - at, out, expected);
}

bool Deflate::Compress(const uint8_t* source, size_t size, std::vector<uint8_t>& out)
{
	if (source == nullptr)
		return false;

	out.clear();
	out.reserve(size / 4 + 64);

	Writer writer = { &out, 0, 0 };
	writer.Bits(1, 1);
	writer.Bits(1, 2);

	std::vector<int> head(kHashSize, -1);
	std::vector<int> prev(size == 0 ? 1 : size, -1);

	size_t at = 0;

	while (at < size)
	{
		int bestLength = 0;
		int bestDistance = 0;

		if (at + kMinMatch <= size)
		{
			const int slot = Hash(source + at);

			int candidate = head[slot];
			int chain = kMaxChain;

			while (candidate >= 0 && chain-- > 0)
			{
				const size_t distance = at - static_cast<size_t>(candidate);
				if (distance == 0 || distance > kWindow)
					break;

				size_t length = 0;
				const size_t limit = size - at < kMaxMatch ? size - at : kMaxMatch;

				while (length < limit && source[candidate + length] == source[at + length])
					++length;

				if (static_cast<int>(length) > bestLength)
				{
					bestLength = static_cast<int>(length);
					bestDistance = static_cast<int>(distance);

					if (bestLength >= kMaxMatch)
						break;
				}

				candidate = prev[candidate];
			}

			prev[at] = head[slot];
			head[slot] = static_cast<int>(at);
		}

		if (bestLength >= kMinMatch)
		{
			const int lengthSymbol = LengthSymbol(bestLength);
			WriteSymbol(writer, 257 + lengthSymbol);
			writer.Bits(static_cast<uint32_t>(bestLength - kLengthBase[lengthSymbol]),
				kLengthExtra[lengthSymbol]);

			const int distanceSymbol = DistanceSymbol(bestDistance);
			writer.Code(static_cast<uint32_t>(distanceSymbol), 5);
			writer.Bits(static_cast<uint32_t>(bestDistance - kDistanceBase[distanceSymbol]),
				kDistanceExtra[distanceSymbol]);

			for (int i = 1; i < bestLength; ++i)
			{
				++at;

				if (at + kMinMatch <= size)
				{
					const int slot = Hash(source + at);
					prev[at] = head[slot];
					head[slot] = static_cast<int>(at);
				}
			}

			++at;
		}
		else
		{
			WriteLiteral(writer, source[at]);
			++at;
		}
	}

	WriteSymbol(writer, 256);
	writer.Flush();

	return true;
}

namespace {

uint32_t Crc32(const uint8_t* source, size_t size)
{
	static uint32_t table[256];
	static bool built = false;

	if (!built)
	{
		for (uint32_t i = 0; i < 256; ++i)
		{
			uint32_t value = i;

			for (int bit = 0; bit < 8; ++bit)
				value = (value & 1u) ? (0xedb88320u ^ (value >> 1)) : (value >> 1);

			table[i] = value;
		}

		built = true;
	}

	uint32_t crc = 0xffffffffu;

	for (size_t i = 0; i < size; ++i)
		crc = table[(crc ^ source[i]) & 0xffu] ^ (crc >> 8);

	return crc ^ 0xffffffffu;
}

void AppendLittle(std::vector<uint8_t>& out, uint32_t value)
{
	out.push_back(static_cast<uint8_t>(value & 0xff));
	out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
	out.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
	out.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

}

bool Deflate::Gzip(const uint8_t* source, size_t size, std::vector<uint8_t>& out)
{
	std::vector<uint8_t> body;
	if (!Compress(source, size, body))
		return false;

	out.clear();
	out.reserve(body.size() + 18);

	const uint8_t header[10] = { 0x1f, 0x8b, 0x08, 0, 0, 0, 0, 0, 0, 0xff };
	out.insert(out.end(), header, header + sizeof(header));
	out.insert(out.end(), body.begin(), body.end());

	AppendLittle(out, Crc32(source, size));
	AppendLittle(out, static_cast<uint32_t>(size));

	return true;
}
