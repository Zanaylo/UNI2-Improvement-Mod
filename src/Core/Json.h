#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Json
{
	class Value
	{
	public:
		enum Type
		{
			Type_Null,
			Type_Bool,
			Type_Number,
			Type_String,
			Type_Array,
			Type_Object
		};

		Type GetType() const { return m_type; }

		bool IsNull() const { return m_type == Type_Null; }
		bool IsArray() const { return m_type == Type_Array; }
		bool IsObject() const { return m_type == Type_Object; }

		const Value* Find(const char* key) const;
		const Value* At(size_t index) const;

		size_t Count() const { return m_items.size(); }

		std::string AsString(const char* fallback = "") const;
		double AsNumber(double fallback = 0.0) const;
		uint64_t AsUnsigned(uint64_t fallback = 0) const;
		bool AsBool(bool fallback = false) const;

		std::string MemberString(const char* key, const char* fallback = "") const;
		uint64_t MemberUnsigned(const char* key, uint64_t fallback = 0) const;
		bool MemberBool(const char* key, bool fallback = false) const;

	private:
		friend class Reader;

		Type m_type = Type_Null;
		bool m_bool = false;
		double m_number = 0.0;
		std::string m_text;
		std::vector<Value> m_items;
		std::vector<std::pair<std::string, Value>> m_members;
	};

	bool Parse(const std::string& text, Value& out);
}
