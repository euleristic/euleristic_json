// Version 1.0.1

// Copyright(c) 2023 Valter Ottenvang
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once
#include <unordered_map>
#include <map>
#include <span>
#include <vector>
#include <string>
#include <string_view>
#include <filesystem>
#include <optional>
#include <variant>
#include <concepts>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <ranges>
#include <iterator>
#include <compare>

// #define EULERISTIC_JSON_COUT before including this file for run-time console output.
#ifdef EULERISTIC_JSON_COUT
#include <iostream>
#define PUSH_TO_COUT(values) std::cout << values
#else //EULERISTIC_JSON_COUT
#define PUSH_TO_COUT(values)
#endif //EULERISTIC_JSON_COUT

// This JSON tool is written in accordance with ECMA-404, 2nd edition.
// NOTE: THIS TOOL ASSUMES char IS UTF-8! If your system implements char differently, it should fail. 
// If you write files with this software however, it can read it.
namespace euleristic::json {

	// A struct representing an error during parsing.
	struct parsing_error {
		enum class type_t {
			UNKNOWN_TOKEN,
			UNEXPECTED_TOKEN,
			UNEXPECTED_SOURCE_END,
			FILE_NOT_FOUND,
			FILE_READ_ERROR,
			INCORRECT_FILE_EXTENSION,
			ILLEGAL_CODE_POINT,
			BAD_REVERSE_SOLIDUS,
			INCORRECT_NUMBER_FORMAT,
			STRING_TYPE_TOO_NARROW,
			INTEGER_TYPE_TOO_NARROW,
			FLOATING_POINT_TYPE_TOO_NARROW
		} type;
		std::optional<uint16_t> line, character;
	};

	// An enum representing an error during formatting.
	enum class format_error {
		ILLEGAL_CODE_POINT,
		CONVERSION_FAILURE
	};

	enum class interface_misuse {
		INCORRECT_TYPE,
		INDEX_OUT_OF_RANGE,
		NO_SUCH_KEY,
		ILLEGAL_OPERAND
	};

	// Concepts
	template <typename S>
	concept string_concept = std::same_as<S, std::string> || std::same_as<S, std::wstring>;

	template <std::integral I, std::floating_point F, string_concept S>
	class value_type;

	// A class which hides implementation details so that the API is cleaner.
	// Basically: DO NOT USE. Unless you really want to. 
	class _tokenizer {
		// Encapsulates a JSON token
		struct _token {
			// The possible types of a JSON token, according to ECMA-404, 2nd ed..
			enum class _type_t {

				// Structural tokens
				LEFT_SQUARE_BRACKET,
				LEFT_CURLY_BRACKET,
				RIGHT_SQUARE_BRACKET,
				RIGHT_CURLY_BRACKET,
				COLON,
				COMMA,


				// Literal name tokens

				TRUE_LITERAL,
				FALSE_LITERAL,
				NULL_LITERAL,


				// Literal type tokens

				STRING_LITERAL,
				NUMBER_LITERAL

			} _type;
			uint16_t _line, _character;
			std::optional<std::string> _value;

		};

		// Transforms the JSON text source into a parsable JSON token sequence.
		// If an unknown token is encountered, the unexpected value is returned, with the token's line and character number.
		static std::vector<_token> _tokenize(const std::string_view source) {

			// A lambda which checks whether a UTF-8 code point is white space
			auto is_white_space = [](const char c) -> bool {
				return c == '\t' || c == '\n' || c == '\r' || c == ' ';
			};

			// A lambda which checks whether a UTF-8 code point is white space or structural
			auto is_token_delimiter = [&is_white_space](const char c) -> bool {
				return is_white_space(c) ||
					c == '[' || c == '{' || c == ']' || c == '}' || c == ':' || c == ','; // or source.end(), needs inline checking.
			};

			// A lambda which checks whether a UTF-8 code point is an allowed part of a digit
			auto is_number_character = [](const char c) -> bool {
				return ('0' <= c && c <= '9') || c == '-' || c == 'e' ||
					c == 'E' || c == '.' || c == '+';
			};

			std::vector<_token> token_sequence;

			uint16_t line = 1;
			uint16_t character = 1;

			auto cursor = source.cbegin();

			while (cursor < source.cend()) {

				// Skip white space
				if (is_white_space(*cursor)) {
					if (*cursor == u'\n') {
						++line;
						character = 1;
					}
					else {
						++character;
					}
					++cursor;
					continue;
				}


				// Identify token
				switch (*cursor) {

					// Structural tokens

				case '[':
					token_sequence.push_back({ _token::_type_t::LEFT_SQUARE_BRACKET, line, character, {} });
					++character;
					++cursor;
					continue;

				case '{':
					token_sequence.push_back({ _token::_type_t::LEFT_CURLY_BRACKET, line, character, {} });
					++character;
					++cursor;
					continue;

				case ']':
					token_sequence.push_back({ _token::_type_t::RIGHT_SQUARE_BRACKET, line, character, {} });
					++character;
					++cursor;
					continue;

				case '}':
					token_sequence.push_back({ _token::_type_t::RIGHT_CURLY_BRACKET, line, character, {} });
					++character;
					++cursor;
					continue;
				case ':':
					token_sequence.push_back({ _token::_type_t::COLON, line, character, {} });
					++character;
					++cursor;
					continue;

				case ',':
					token_sequence.push_back({ _token::_type_t::COMMA, line, character, {} });
					++character;
					++cursor;
					continue;

					// Literal name tokens

				case 't':
					if (source.cend() - cursor < 4) {
						throw parsing_error{ parsing_error::type_t::UNKNOWN_TOKEN, line, character };
					}
					else if (!is_token_delimiter(*(cursor + 4))) {
						throw parsing_error{ parsing_error::type_t::UNKNOWN_TOKEN, line, character };
					}
					if (std::string_view(cursor, cursor + 4) != "true") {
						throw parsing_error{ parsing_error::type_t::UNKNOWN_TOKEN, line, character };
					}
					token_sequence.push_back({ _token::_type_t::TRUE_LITERAL, line, character, {} });
					character += 4;
					cursor += 4;
					continue;

				case 'f':
					if (source.cend() - cursor < 5) {
						throw parsing_error{ parsing_error::type_t::UNKNOWN_TOKEN, line, character };
					}
					else if (!is_token_delimiter(*(cursor + 5))) {
						throw parsing_error{ parsing_error::type_t::UNKNOWN_TOKEN, line, character };
					}
					if (std::string_view(cursor, cursor + 5) != "false") {
						throw parsing_error{ parsing_error::type_t::UNKNOWN_TOKEN, line, character };
					}
					token_sequence.push_back({ _token::_type_t::FALSE_LITERAL, line, character, {} });
					character += 5;
					cursor += 5;
					continue;

				case 'n':
					if (source.cend() - cursor < 4) {
						throw parsing_error{ parsing_error::type_t::UNKNOWN_TOKEN, line, character };
					}
					else if (!is_token_delimiter(*(cursor + 4))) {
						throw parsing_error{ parsing_error::type_t::UNKNOWN_TOKEN, line, character };
					}
					if (std::string_view(cursor, cursor + 4) != "null") {
						throw parsing_error{ parsing_error::type_t::UNKNOWN_TOKEN, line, character };
					}
					token_sequence.push_back({ _token::_type_t::NULL_LITERAL, line, character, {} });
					character += 4;
					cursor += 4;
					continue;


					// Value tokens

				case '\"': {

					// The tokenizer will not ensure the validity of the string, only that it has a valid start and end.

					auto peeker = cursor + 1;
					while (*peeker != '\"') {
						if (*peeker == '\\') {
							++peeker;
							if (peeker == source.cend()) {
								throw parsing_error{ parsing_error::type_t::UNEXPECTED_SOURCE_END, line, character };
							}
						}
						++peeker;
						if (peeker == source.cend()) {
							throw parsing_error{ parsing_error::type_t::UNEXPECTED_SOURCE_END, line, character };
						}
					}
					token_sequence.push_back({ _token::_type_t::STRING_LITERAL, line, character, std::string(cursor + 1, peeker) });
					character += 1 + static_cast<uint16_t>(peeker - cursor);
					cursor = peeker + 1;
					continue;
				}

				case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
				case '8': case '9': case '-': case '.': {

					auto peeker = cursor;
					while (peeker != source.cend() && is_number_character(*peeker)) {
						++peeker;
					}
					token_sequence.push_back({ _token::_type_t::NUMBER_LITERAL, line, character, std::string(cursor, peeker) });
					character += static_cast<uint16_t>(peeker - cursor);
					cursor = peeker;
					continue;
				}

				default:
					throw parsing_error{ parsing_error::type_t::UNKNOWN_TOKEN, line, character };
				}
			}
			return token_sequence;
		}

		// Friends
		template <std::integral I, std::floating_point F, string_concept S> 
		friend class value_type;
		template <std::integral I, std::floating_point F, string_concept S> 
		friend value_type<I, F, S> parse_text(const std::string_view);
	};

	// A class which hides implementation details so that the API is cleaner.
	// Basically: DO NOT USE. Unless you really want to. 
	class _string_handler {

		// Facet for wide/narrow string conversion 
		[[maybe_unused]] inline static const auto& _code_conv_facet =
			std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(std::locale());

		// Parses the string from JSON formatting
		template <string_concept string_type>
		static string_type _parse_string(const std::string_view input, const uint16_t line, const uint16_t character);

		// Formats the string to legal JSON
		template <string_concept string_type>
		static std::string _format_string(const std::basic_string_view<typename string_type::value_type> input);

		// Friends
		template <std::integral I, std::floating_point F, string_concept S> friend class value_type;

	};

	// Parses the string and converts it to wide.
	template <>
	std::wstring _string_handler::_parse_string(const std::string_view input, const uint16_t line, const uint16_t character) {

		// Check for control characters
		auto ctrl_char = std::find_if(input.cbegin(), input.cend(), [](const char c) { return c <= 0x1F; });
		if (ctrl_char != input.cend()) {
			throw parsing_error{ parsing_error::type_t::ILLEGAL_CODE_POINT, line, character + static_cast<uint16_t>(ctrl_char - input.cbegin())};
		}

		// Set up variables

		std::wstring output(input.size(), L'\0');
		const char* cursor = input.data();
		const char* end = input.data() + input.size();
		wchar_t* wcursor = output.data();
		wchar_t* wend = output.data() + input.size();
		mbstate_t mbstate{};

		// Loop until whole string is parsed
		while (cursor != end) {
			auto reverse_solidus = std::find(cursor, end, '\\');
			auto result = _code_conv_facet.in(mbstate, cursor, reverse_solidus, cursor,
				wcursor, wend, wcursor);
			if (result != std::codecvt_base::ok) {
				throw parsing_error{ parsing_error::type_t::ILLEGAL_CODE_POINT, line, character + static_cast<uint16_t>(cursor - input.data()) };
			}
			if (cursor != end) {
				++cursor;
				if (cursor == end) {
					throw parsing_error{ parsing_error::type_t::BAD_REVERSE_SOLIDUS, line, character + static_cast<uint16_t>(cursor - input.data()) };
				}

				switch (*cursor) {
				case '\"':
					*wcursor = L'\"';
					break;
				case '\\':
					*wcursor = L'\\';
					break;
				case '/':
					*wcursor = L'/';
					break;
				case 'b':
					*wcursor = L'\b';
					break;
				case 'f':
					*wcursor = L'\f';
					break;
				case 'n':
					*wcursor = L'\n';
					break;
				case 'r':
					*wcursor = L'\r';
					break;
				case 't':
					*wcursor = L'\t';
					break;
				case 'u':
					++cursor;
					if (cursor > end - 4) {
						throw parsing_error{ parsing_error::type_t::BAD_REVERSE_SOLIDUS, line, character + static_cast<uint16_t>(cursor - input.data()) };
					}
					uint16_t code;
					try {
						code = static_cast<uint16_t>(std::stoul(std::string(cursor, cursor + 4), nullptr, 16));
					} catch (std::invalid_argument error) {
						PUSH_TO_COUT("Code point at (" << line << ", " << character + static_cast<uint16_t>(cursor - input.data()) << ") was poorly formatted.\n");
						throw parsing_error{ parsing_error::type_t::BAD_REVERSE_SOLIDUS, line, character + static_cast<uint16_t>(cursor - input.data()) };
					}
					*wcursor = static_cast<wchar_t>(code);
					cursor += 4;
					break;
				default:
					throw parsing_error{ parsing_error::type_t::BAD_REVERSE_SOLIDUS, line, character + static_cast<uint16_t>(cursor - input.data()) };
					break;
				}
				++wcursor;
			}
		}

		return std::wstring(output.data(), wcursor);
	}

	// Parses the string without conversion.
	template <>
	std::string _string_handler::_parse_string<std::string>(const std::string_view input, const uint16_t line, const uint16_t character) {
		std::string output;
		for (auto it = input.cbegin(); it != input.cend(); ++it) {

			// Control characters are not allowed
			if (*it <= 0x1F) {
				throw parsing_error{ parsing_error::type_t::ILLEGAL_CODE_POINT, line, character + static_cast<uint16_t>(it - input.cbegin()) };
			}

			// Handle '\'
			if (*it == '\\') {
				++it;

				// Did the string end?
				if (it == input.cend()) {
					throw parsing_error{ parsing_error::type_t::BAD_REVERSE_SOLIDUS, line, character + static_cast<uint16_t>(it - input.cbegin()) };
				}

				switch (*it) {

				case '\"':
					output.append(1, '\"');
					break;
				case '\\':
					output.append(1, '\\');
					break;
				case '/':
					output.append(1, '/');
					break;
				case 'b':
					output.append(1, '\b');
					break;
				case 'f':
					output.append(1, '\f');
					break;
				case 'n':
					output.append(1, '\n');
					break;
				case 'r':
					output.append(1, '\r');
					break;
				case 't':
					output.append(1, '\t');
					break;
				case 'u':
					++it;
					if (it > input.cend() - 4) {
						throw parsing_error{ parsing_error::type_t::BAD_REVERSE_SOLIDUS, line, character + static_cast<uint16_t>(it - input.cbegin()) };
					}
					uint16_t code;
					try {
						code = static_cast<uint16_t>(std::stoul(std::string(it, it + 4), nullptr, 16));
					} catch (std::invalid_argument error) {
						PUSH_TO_COUT("Code point at (" << line << ", " << character + static_cast<uint16_t>(it - input.cbegin()) << ") was poorly formatted.\n");
						throw parsing_error{ parsing_error::type_t::BAD_REVERSE_SOLIDUS, line, character + static_cast<uint16_t>(it - input.cbegin()) };
					}
					if (code > 0xFF) {
						PUSH_TO_COUT("Code point at (" << line << ", " << character + static_cast<uint16_t>(it - input.cbegin()) << ") was out of range of char\n");
						throw parsing_error{ parsing_error::type_t::STRING_TYPE_TOO_NARROW, line, character + static_cast<uint16_t>(it - input.cbegin()) + static_cast<uint16_t>(it - input.cbegin()) };
					}
					output.append(1, static_cast<char>(code));
					it += 3; // for loop will iterate again
					break;
				default:
					throw parsing_error{ parsing_error::type_t::BAD_REVERSE_SOLIDUS, line, character + static_cast<uint16_t>(it - input.cbegin()) };
				}

				continue;
			}

			output.append(1, *it);
		}

		return output;
	}

	// Formats and converts the string to narrow.
	template <>
	std::string _string_handler::_format_string<std::wstring>(std::wstring_view input) {

		// Set up variables

		std::string output(input.size(), '\0');
		const wchar_t* wcursor = input.data();
		const wchar_t* wend = input.data() + input.size();
		char* cursor;
		size_t cursor_offset = 0;
		size_t end_offset = input.size();
		mbstate_t mbstate{};

		// Loop until whole string is parsed

		while (wcursor != wend) {
			auto ctrl_char = std::find_if(wcursor, wend, [](const wchar_t wc) {
				if (wc == L'\"') return true;
				if (wc == L'\\') return true;
				if (wc == L'\b') return true;
				if (wc == L'\f') return true;
				if (wc == L'\n') return true;
				if (wc == L'\r') return true;
				if (wc == L'\t') return true;
				if (wc <= 0x1F) throw format_error::ILLEGAL_CODE_POINT;
				return false;
			});
			auto result = _code_conv_facet.out(mbstate, wcursor, ctrl_char, wcursor,
				&output[cursor_offset], &output[end_offset], cursor);

			if (wcursor == wend) {
				break;
			}
			cursor_offset = cursor - output.data();
			switch (result) {

				// Code point was wider than fits char, so it must be represented with hex
			case std::codecvt_base::error: {
				// Handle UTF-32 if needed. If wchar_t is larger, bits over 0xFFFFFFFF are ignored.
				if constexpr (sizeof(wchar_t) >= 4) {
					uint16_t code = static_cast<uint16_t>(static_cast<uint32_t>(*wcursor) >> 16);
					std::stringstream ss;
					ss << std::hex << code;
					std::string to_insert;
					ss >> to_insert;
					while (to_insert.size() < 4) to_insert.insert(to_insert.begin(), '0');
					to_insert.insert(0, "\\u");
					output.insert(cursor_offset, to_insert);
					cursor_offset += 6;
					end_offset += 6;

				}
				uint16_t code = static_cast<uint16_t>(*wcursor);
				std::stringstream ss;
				ss << std::hex << code;
				std::string to_insert;
				ss >> to_insert;
				while (to_insert.size() < 4) to_insert.insert(to_insert.begin(), '0');
				to_insert.insert(0, "\\u");
				output.insert(cursor_offset, to_insert);
				cursor_offset += 6;
				end_offset += 6;
				++wcursor;
				break;
			}

				// Code point is control character, which we must enter with a '\'
			case std::codecvt_base::ok: {
				switch (*wcursor) {
				case L'\"': output.insert(cursor_offset, "\\\""); break;
				case L'\\': output.insert(cursor_offset, "\\\\"); break;
				case L'\b': output.insert(cursor_offset, "\\b");  break;
				case L'\f': output.insert(cursor_offset, "\\f");  break;
				case L'\n': output.insert(cursor_offset, "\\n");  break;
				case L'\r': output.insert(cursor_offset, "\\r");  break;
				case L'\t': output.insert(cursor_offset, "\\t");  break;
				}
				cursor_offset += 2;
				end_offset += 2;
				++wcursor;
				break;
			}

			case std::codecvt_base::partial:
				throw format_error::CONVERSION_FAILURE;
			}

		}
		return output;
	}

	// Formats the string without conversion.
	template <>
	std::string _string_handler::_format_string<std::string>(std::string_view input) {
		return std::accumulate(input.cbegin(), input.cend(), std::string(), [](std::string str, const char c) {
			if (c == '\"') return str + "\\\"";
			if (c == '\\') return str + "\\\\";
			if (c == '\b') return str + "\\b";
			if (c == '\f') return str + "\\f";
			if (c == '\n') return str + "\\n";
			if (c == '\r') return str + "\\r";
			if (c == '\t') return str + "\\t";
			if (c <= 0x1F) throw format_error::ILLEGAL_CODE_POINT;
			return str + c;
		});
	}


	// Wraps a JSON value and provides an interface for access and modification of it, given the user provided C++ types. If null, the json_value is not set.
	template <std::integral integer_type = int, std::floating_point floating_point_type = float, string_concept string_type = std::string>
	class value_type {

		// Aliases for brevity.

		using _object_alias = std::unordered_map<string_type, value_type<integer_type, floating_point_type, string_type>>;
		using _array_alias = std::vector<value_type>;
		using _value_alias = std::optional<std::variant<_object_alias, _array_alias, integer_type, floating_point_type, string_type, bool>>;

		// Underlying value.
		_value_alias _value;

		// Currently held type.
		enum class _type_t {
			NULL_VALUE, OBJECT, ARRAY, INTEGER, FLOATING_POINT, STRING, BOOLEAN
		} _type;

		// Friends.
		template <std::integral I, std::floating_point F, string_concept S>
		friend value_type<I, F, S> parse_text(std::string_view);
		template <std::integral I, std::floating_point F, string_concept S>
		friend std::ostream& operator<<(std::ostream&, const value_type<I, F, S>&);
		template <std::integral I, std::floating_point F, string_concept S>
		friend std::partial_ordering operator<=>(const value_type<I, F, S>& lhs, const value_type<I, F, S>& rhs);

		// Writes the value to the stream in JSON at indentation level depth.
		void _write_to_ostream(std::ostream& stream, size_t depth = 0) const {
			auto indent = [&stream](size_t depth) {
				for (size_t i = 0; i < depth; ++i) stream << '\t';
			};

			switch (_type) {
			case _type_t::ARRAY: {

				auto& arr = std::get<_array_alias>(*_value);
				stream << '[';
				if (arr.empty()) {
					stream << ']';
					return;
				}
				else {
					stream << '\n';
					for (auto it = arr.begin(); it != arr.end(); ++it) {
						indent(depth + 1);
						it->_write_to_ostream(stream, depth + 1);
						if (std::next(it) != arr.end()) {
							stream << ",\n";
						}
					}
					stream << '\n';
					indent(depth);
					stream << ']';
				}
				return;
			}

			case _type_t::OBJECT: {
				auto& obj = std::get<_object_alias>(*_value);
				stream << '{';
				if (obj.empty()) {
					stream << '}';
					return;
				}
				else {
					stream << '\n';
					for (auto it = obj.begin(); it != obj.end(); ++it) {
						indent(depth + 1); // i love u baby. u are my wife.
						auto& [key, value] = *it;
						stream << '\"' << _string_handler::_format_string<string_type>(key) << "\": ";
						value._write_to_ostream(stream, depth + 1);
						if (std::next(it) != obj.end()) {
							stream << ",\n";
						}
					}
					stream << '\n';
					indent(depth);
					stream << '}';
				}
				return;
			}

			case _type_t::STRING: {
				stream << '\"' << _string_handler::_format_string<string_type>(std::get<string_type>(*_value)) << '\"';
				return;
			}
			case _type_t::INTEGER:
				if constexpr (!std::is_same_v<integer_type, int>
					|| !std::is_same_v<integer_type, long>
					|| !std::is_same_v<integer_type, long long>
					|| !std::is_same_v<integer_type, unsigned int>
					|| !std::is_same_v<integer_type, unsigned long>
					|| !std::is_same_v<integer_type, unsigned long long>)
					stream << std::to_string(std::get<integer_type>(*_value));
				else
					stream << std::to_string(static_cast<long long>(std::get<integer_type>(*_value)));
				return;
			case _type_t::FLOATING_POINT:
				stream << std::to_string(std::get<floating_point_type>(*_value));
				return;
			case _type_t::BOOLEAN:
				if (std::get<bool>(*_value))
					stream << "true";
				else
					stream << "false";
				return;
			case _type_t::NULL_VALUE:
				stream << "null";
				return;
			default:
				stream << "default";
				return;
			}
		};

		// Parses the value beginning at cursor and ending some time before end.
		// Cursor should after returning be one element past the value (e.g. past ] or }).
		value_type(std::vector<_tokenizer::_token>::const_iterator& cursor, const std::vector<_tokenizer::_token>::const_iterator end) {
			// The token sequence goes depth first, so it's how it will be parsed recursively.

			switch (cursor->_type) {

				// Value is array
			case _tokenizer::_token::_type_t::LEFT_SQUARE_BRACKET: {
				_type = _type_t::ARRAY;
				_array_alias arr;
				++cursor;

				// Is the array empty?
				if (cursor->_type == _tokenizer::_token::_type_t::RIGHT_SQUARE_BRACKET) {
					++cursor;
					_value = arr;
					return;
				}

				// Parse the array
				while (cursor < end) {
					arr.push_back(value_type<integer_type, floating_point_type, string_type>(cursor, end));
					if (cursor != end) {
						if (cursor->_type == _tokenizer::_token::_type_t::COMMA) {
							++cursor;
							continue;
						}
						else if (cursor->_type == _tokenizer::_token::_type_t::RIGHT_SQUARE_BRACKET) {
							++cursor;
							_value = arr;
							return;
						}
						else {
							PUSH_TO_COUT("Unexpected token encountered at (" << cursor->_line << ", " << cursor->_character << ")\n");
							throw parsing_error{ parsing_error::type_t::UNEXPECTED_TOKEN, cursor->_line, cursor->_character };
						}
					}
				}
				PUSH_TO_COUT("Source ended unexpectedly before an array was completely parsed.\n");
				throw parsing_error{ parsing_error::type_t::UNEXPECTED_SOURCE_END, {}, {} };
			}

				// Value is object
			case _tokenizer::_token::_type_t::LEFT_CURLY_BRACKET: {
				_type = _type_t::OBJECT;
				_object_alias obj;
				++cursor;

				// Is the object empty?
				if (cursor->_type == _tokenizer::_token::_type_t::RIGHT_CURLY_BRACKET) {
					++cursor;
					_value = obj;
					return;
				}

				// Parse the object
				while (cursor < end) {

					// key
					if (cursor->_type != _tokenizer::_token::_type_t::STRING_LITERAL) {
						PUSH_TO_COUT("Unexpected token encountered at (" << cursor->_line << ", " << cursor->_character << "), expected string literal.\n");
						throw parsing_error{ parsing_error::type_t::UNEXPECTED_TOKEN, cursor->_line, cursor->_character };
					}
					auto key = _string_handler::_parse_string<string_type>(*cursor->_value, cursor->_line, cursor->_character);
					++cursor;

					// colon
					if (cursor == end) break;
					if (cursor->_type != _tokenizer::_token::_type_t::COLON) {
						PUSH_TO_COUT("Unexpected token encountered at (" << cursor->_line << ", " << cursor->_character << "), expected ':'.\n");
						throw parsing_error{ parsing_error::type_t::UNEXPECTED_TOKEN, cursor->_line, cursor->_character };
					}
					++cursor;

					// "value"
					if (cursor == end) break;
					obj[key] = value_type<integer_type, floating_point_type, string_type>(cursor, end);

					// comma or right curly bracket
					if (cursor == end) break;
					if (cursor->_type == _tokenizer::_token::_type_t::RIGHT_CURLY_BRACKET) {
						++cursor;
						_value = obj;
						return;
					}
					if (cursor->_type != _tokenizer::_token::_type_t::COMMA) break;
					++cursor;
				}
				PUSH_TO_COUT("Source ended unexpectedly before an object was completely parsed.\n");
				throw parsing_error{ parsing_error::type_t::UNEXPECTED_SOURCE_END, {}, {} };
			}

				// Value is number
			case _tokenizer::_token::_type_t::NUMBER_LITERAL: {

				// cursor is expected to be the next token, so we hang on to the number token and iterate cursor before evaluating the number
				auto number = cursor;
				++cursor;
				try {
					// Floating point?
					if (std::any_of(number->_value->cbegin(), number->_value->cend(), [](const char c) { return c == '.'; })) {
						_type = _type_t::FLOATING_POINT;
						try {
							if constexpr (std::is_same_v<floating_point_type, long double>) {
								_value = std::stold(*number->_value);
							}
							else if constexpr (std::is_same_v<floating_point_type, double>) {
								_value = std::stod(*number->_value);
							}
							else if constexpr (std::is_same_v<floating_point_type, float>) {
								_value = std::stof(*number->_value);
							}
							else {
								// Those are all floating point types, but should there ever exist a new one:
								_value = static_cast<floating_point_type>(std::stold(*number->_value));
							}
							return;
						} catch (std::out_of_range error) {
							PUSH_TO_COUT("Number token at (" << number->_line << ", " << number->_character << ") was out of range for floating_point_type.\n");
							throw parsing_error{ parsing_error::type_t::FLOATING_POINT_TYPE_TOO_NARROW, number->_line, number->_character };
						}
					}

					_type = _type_t::INTEGER;
					// Integer
					try {
						if constexpr (std::is_same_v<integer_type, long long>) {
							_value = std::stol(*number->_value);
						}
						else if constexpr (std::is_same_v<integer_type, long>) {
							_value = std::stol(*number->_value);
						}
						else if constexpr (std::is_same_v<integer_type, int>) {
							_value = std::stoi(*number->_value);
						}
						else if constexpr (std::is_same_v<integer_type, unsigned long long>) {
							_value = std::stoull(*number->_value);
						}
						else if constexpr (std::is_same_v<integer_type, unsigned long>) {
							_value = std::stoul(*number->_value);
						}
						else {
							// Default to long long, as to be inclusive.
							_value = static_cast<integer_type>(std::stoll(*number->_value));
						}
						return;

					} catch (std::out_of_range error) {
						PUSH_TO_COUT("Number token at (" << number->_line << ", " << number->_character << ") was out of range for integer_type.\n");
						throw parsing_error{ parsing_error::type_t::INTEGER_TYPE_TOO_NARROW, number->_line, number->_character };
					}

				} catch (std::invalid_argument error) {
					PUSH_TO_COUT("Number token at (" << number->_line << ", " << number->_character << ") was of incorrect format.\n");
					throw parsing_error{ parsing_error::type_t::INCORRECT_NUMBER_FORMAT, number->_line, number->_character };
				}
			}

			case _tokenizer::_token::_type_t::STRING_LITERAL: {
				_type = _type_t::STRING;
				_value = _string_handler::_parse_string<string_type>(*cursor->_value, cursor->_line, cursor->_character);
				++cursor;
				return;
			}

			case _tokenizer::_token::_type_t::TRUE_LITERAL:
				_type = _type_t::BOOLEAN;
				_value = true;
				++cursor;
				return;

			case _tokenizer::_token::_type_t::FALSE_LITERAL:
				_type = _type_t::BOOLEAN;
				_value = false;
				++cursor;
				return;

			case _tokenizer::_token::_type_t::NULL_LITERAL:
				_type = _type_t::NULL_VALUE;
				_value = {};
				++cursor;
				return;

			default:
				throw parsing_error{ parsing_error::type_t::UNEXPECTED_TOKEN, cursor->_line, cursor->_character };
			}
		}

	public:
		// Default is JSON value null.
		value_type() : _value({}), _type(_type_t::NULL_VALUE) {}

		// Interface

		// Constructs a JSON object from a std::unordered_map.
		value_type(const std::unordered_map<string_type, value_type<integer_type, floating_point_type, string_type>>& obj) {
			_type = _type_t::OBJECT;
			_value = obj;
		}

		// Constructs a JSON object from a std::map.
		value_type(const std::map<string_type, value_type<integer_type, floating_point_type, string_type>>& obj) {
			_type = _type_t::OBJECT;
			_value = std::unordered_map<string_type, value_type<integer_type, floating_point_type, string_type>>(obj.cbegin(), obj.cend());
		}

		// Constructs a JSON array from a std::array.
		template <size_t size>
		value_type(const std::array<value_type<integer_type, floating_point_type, string_type>, size> arr) {
			_type = _type_t::ARRAY;
			_value = std::vector<value_type>(arr.cbegin(), arr.cend());
		}

		// Constructs a JSON array from a std::vector.
		value_type(const std::vector<value_type<integer_type, floating_point_type, string_type>> vec) {
			_type = _type_t::ARRAY;
			_value = vec;
		}

		// Constructs a JSON string from a string
		template <std::convertible_to<string_type> string_in>
		value_type(const string_in str) {
			_type = _type_t::STRING;
			_value = str;
		}

		// Constructs a JSON number from an integer
		template <std::integral integer_in>
		value_type(const integer_in n) {
			_type = _type_t::INTEGER;
			_value = n;
		}

		// Constructs a JSON number from an floating point
		template <std::floating_point floating_point_in>
		value_type(const floating_point_in f) {
			_type = _type_t::FLOATING_POINT;
			_value = f;
		}

		// Constructs a JSON number from a boolean
		value_type(const bool b) {
			_type = _type_t::BOOLEAN;
			_value = b;
		}

		// Constructs a JSON null value
		value_type(const nullptr_t) {
			_type = _type_t::NULL_VALUE;
			_value = {};
		}

		// If this is an array, returns the value at index.
		[[nodiscard]] const value_type<integer_type, floating_point_type, string_type>& operator[](const size_t index) const {
			if (_type != _type_t::ARRAY) {
				throw interface_misuse::INCORRECT_TYPE;
			}
			auto& arr = std::get<_array_alias>(*_value);
			if (index >= arr.size()) {
				throw interface_misuse::INDEX_OUT_OF_RANGE;
			}
			return arr.at(index);
		}

		// If this is an object, returns the value at key.
		[[nodiscard]] const value_type<integer_type, floating_point_type, string_type>&
			operator[](const string_type& key) const {
			if (_type != _type_t::OBJECT) {
				throw interface_misuse::INCORRECT_TYPE;
			}
			auto& obj = std::get<_object_alias>(*_value);
			if (!obj.contains(key)) {
				throw interface_misuse::NO_SUCH_KEY;
			}
			return obj.at(key);
		}

		[[nodiscard]] operator bool() const {
			return _value.has_value();
		}

		// Returns whether this is value null
		[[nodiscard]] bool is_null() const {
			return !_value;
		}

		// Returns the value of this, if it is boolean
		[[nodiscard]] bool as_bool() const {
			if (_type != _type_t::BOOLEAN) {
				throw interface_misuse::INCORRECT_TYPE;
			}
			return std::get<bool>(*_value);
		}

		// Returns the value of this, if it is integral
		[[nodiscard]] integer_type as_integer() const {
			if (_type != _type_t::INTEGER) {
				throw interface_misuse::INCORRECT_TYPE;
			}
			return std::get<integer_type>(*_value);
		}

		// Returns the value of this, if it is floating point
		[[nodiscard]] floating_point_type as_floating_point() const {
			if (_type != _type_t::FLOATING_POINT) {
				throw interface_misuse::INCORRECT_TYPE;
			}
			return std::get<floating_point_type>(*_value);
		}

		// Returns the value of this, if it is a string
		[[nodiscard]] string_type as_string() const {
			if (_type != _type_t::STRING) {
				throw interface_misuse::INCORRECT_TYPE;
			}
			return std::get<string_type>(*_value);
		}

		// Returns the value of this as a view, if it is an array
		[[nodiscard]] auto as_array() const {
			if (_type != _type_t::ARRAY) {
				throw interface_misuse::INCORRECT_TYPE;
			}
			auto& arr = std::get<_array_alias>(*_value);
			return std::span{ arr.cbegin(), arr.size() };
		}

		// Returns the value of this, if it is an object
		[[nodiscard]] auto as_object() const {
			if (_type != _type_t::OBJECT) {
				throw interface_misuse::INCORRECT_TYPE;
			}
			auto& obj = std::get<_object_alias>(*_value);
			return std::span{ obj.cbegin(), obj.size() };
		}
	};

	template <std::integral integer_type, std::floating_point floating_point_type, string_concept string_type>
	[[nodiscard]] std::partial_ordering operator<=>(const value_type<integer_type, floating_point_type, string_type>& lhs,
		const value_type<integer_type, floating_point_type, string_type>& rhs) {
		if (lhs._type != rhs._type) {
			throw interface_misuse::INCORRECT_TYPE;
		}
		switch (lhs._type) {
		case value_type<integer_type, floating_point_type, string_type>::_type_t::NULL_VALUE:     throw interface_misuse::ILLEGAL_OPERAND;
		case value_type<integer_type, floating_point_type, string_type>::_type_t::BOOLEAN:		  throw interface_misuse::ILLEGAL_OPERAND;
		case value_type<integer_type, floating_point_type, string_type>::_type_t::OBJECT:         throw interface_misuse::ILLEGAL_OPERAND;
		case value_type<integer_type, floating_point_type, string_type>::_type_t::ARRAY:		  throw interface_misuse::ILLEGAL_OPERAND;
		case value_type<integer_type, floating_point_type, string_type>::_type_t::INTEGER:		  return lhs.as_integer() <=> rhs.as_integer();
		case value_type<integer_type, floating_point_type, string_type>::_type_t::FLOATING_POINT: return lhs.as_floating_point() <=> rhs.as_floating_point();
		case value_type<integer_type, floating_point_type, string_type>::_type_t::STRING:		  return lhs.as_string() <=> rhs.as_string();
		}
	};

	// Parses JSON source text.
	template <std::integral integer_type = int, std::floating_point floating_point_type = float, string_concept string_type = std::string>
	[[nodiscard]] value_type<integer_type, floating_point_type, string_type> parse_text(const std::string_view source) {

		if (source.empty()) {
			PUSH_TO_COUT("Source was empty!");
			throw parsing_error{ parsing_error::type_t::UNEXPECTED_SOURCE_END, {}, {} };
		}

		auto token_sequence = _tokenizer::_tokenize(source);

		// Keep cursor so that we can know if all tokens were parsed
		auto cursor = token_sequence.cbegin();
		auto value = value_type<integer_type, floating_point_type, string_type>(cursor, token_sequence.cend());

		if (cursor != token_sequence.cend()) {
			PUSH_TO_COUT("Unexpected token at (" << cursor->_line << ", " << cursor->_character << "), the source already had a value but continued.\n");
			throw parsing_error{ parsing_error::type_t::UNEXPECTED_TOKEN, cursor->_line, cursor->_character };
		}

		PUSH_TO_COUT("Source was successfully parsed.\n");

		return value;
	};

	// Reads the JSON file at path and calls parse_text with the read source text.
	template <std::integral integer_type = int, std::floating_point floating_point_type = float, string_concept string_type = std::string>
	[[nodiscard]] value_type<integer_type, floating_point_type, string_type> parse_file(const std::filesystem::path path) {
		if (path.extension() != ".json") {
			PUSH_TO_COUT("Unexpected file extension of path: " << path << ", expected .json");
			throw parsing_error{ parsing_error::type_t::INCORRECT_FILE_EXTENSION, {}, {} };
		}

		std::error_code ec;

		if (!std::filesystem::exists(path, ec)) {
			PUSH_TO_COUT("No file found at path: " << path << "\nstd::filesystem::exists gave error code " << ec.value() << ':' << ec.message());
			throw parsing_error{ parsing_error::type_t::FILE_NOT_FOUND, {}, {} };
		};

		PUSH_TO_COUT("Reading file: " << path << '\n');
		std::ifstream file(path);

		if (!file.is_open()) {
			PUSH_TO_COUT("Could not read file.");
			throw parsing_error{ parsing_error::type_t::FILE_READ_ERROR, {}, {} };
		}

		std::basic_stringstream<char> buffer;
		buffer << file.rdbuf();


		return parse_text<integer_type, floating_point_type, string_type>(buffer.view());
	};

	// Writes a JSON value to the file at path.
	template <std::integral integer_type = int, std::floating_point floating_point_type = float, string_concept string_type = std::string>
	void write_to_file(const value_type<integer_type, floating_point_type, string_type>& value, const std::filesystem::path path) {

		PUSH_TO_COUT("Writing to file: " << path << '\n');
		std::ofstream file(path);
		file << value;

		PUSH_TO_COUT("Successfully wrote to file!\n");
	};

	// Writes the value in JSON format to a std::ostream, such as std::ofstream or std::cout.
	template <std::integral integer_type, std::floating_point floating_point_type, string_concept string_type>
	std::ostream& operator<<(std::ostream& stream, const value_type<integer_type, floating_point_type, string_type>& value) {
		value._write_to_ostream(stream);
		return stream;
	};
}

#undef PUSH_TO_COUT
