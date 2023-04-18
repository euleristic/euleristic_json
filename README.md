# euleristic_json
A simple JSON tool for C++20.

## Documentation of the interface
The interface lives entirely within the `euleristic` namespace, but its qualifier is omitted here for brevity.
The tool is templated, allowing the user to specify with what C++ types to store JSON values. The full template parameter list looks like: `template <std::integral integer_type = int, std::floating_point floating_point_type = float, string_concept string_type = std::string>`, but for brevity this documentation will simply say `template <...>`. Currently (atleast until C++ provides further support to character encodings), `string_concept` must be either `std::string` or `std::wstring`.
### `template <...> json::value_type<...> json::parse_file(const std::filesystem::path path)`
Parses the JSON file at `path` and returns the `value_type` it evaluates to. Calls `parse_text`.
### `template <...> json::value_type<...> json::parse_text(const std::string_view source)`
Parses the source as JSON and returns the `value_type` it evaluates to.
### `template <...> void json::write_to_file(const value_type<...> value, const std::filesystem::path path)`
Writes `value` to a file at `path` as JSON.
### `template <...> std::ostream& operator<<(std::ostream& stream, const value_type<...>& value)`
Writes `value` to `stream` as JSON.
### `template <...> class json::value_type`
A class which wraps a JSON value and represents its numbers with `integer_type` and/or `floating_point_type`, and its strings with `string_type`. It is through the interface of this class that the user may query JSON source or write it to file.
#### Copy Constructors
The class has a set of copy constructors which will interpret the given C++ representation of the value as a JSON value. An argument may be `std::map<string_type, json_value<...>>` or `std::unordered_map<string_type, json_value<...>>`, in which case it will be interpreted as a JSON object, `std::vector<json_value<...>>` or `std::array<json_value<...>>`, in which case it will be interpreted as a JSON array, `std::integral`, `std::floating_point`, `std::convertible_to<std::string>`, `bool` or `nullptr`.
#### `[[nodiscard]] /*typename*/ json::value_type::as_*() const`
The class has a set of methods of this format: `as_bool()`, which returns `true` or `false` if the held JSON value is either of these, `as_integer()`, `as_floating_point()` and `as_string()`, which returns the held value as its respective template argument, if the it is of that type, `as_range()`, which returns a `std::span<json_value<...>>` of the held JSON array, if it is an array, and `as_object(), which returns a `std::span<string_type, json_value<...>>` of the held JSON object, if it is an object.
#### `[[nodiscard]] bool json::value_type::is_null() const`
Returns whether the held JSON value is null.
#### `[[nodiscard]] bool operator bool() const`
`*this` evaluates to true if the held JSON value is not null.
#### `[[nodiscard]] const json_value<...>& json::value_type::operator[](const size_t index)`
If `this` holds a JSON array, return the JSON value at index as a `value_type`.
#### `[[nodiscard]] const json_value<...>& json::value_type::operator[](const string_type& key)`
If `this` holds a JSON object, return the JSON value of the kv-pair of key, as a `value_type`.
### `struct json::parsing_error`
This struct is thrown if an error is encountered during parsing.
#### `enum class json::parsing_error::type_t`
An enum class which holds the type of the parsing error, which may be any of:
`UNKNOWN_TOKEN`, `UNEXPECTED_TOKEN`, `UNEXPECTED_SOURCE_END`, `FILE_NOT_FOUND`, `FILE_READ_ERROR`, `INCORRECT_FILE_EXTENSION`, `ILLEGAL_CODE_POINT`, `BAD_REVERSE_SOLIDUS`, `INCORRECT_NUMBER_FORMAT`, `STRING_TYPE_TOO_NARROW`, `INTEGER_TYPE_TOO_NARROW` or ` FLOATING_POINT_TYPE_TOO_NARROW`.
#### `enum class json::parsing_error::type_t json::parsing_error::type`
The type of the parsing error instance.
#### `std::optional<uint16> json::parsing_error::line, json::parsing_error::character`
The line and character(ish) of the JSON source where the parsing error was encountered, if applicable.
### `enum class json::format_error`
This enum is thrown if a formatting error is encountered, and may be either: `ILLEGAL_CODE_POINT` or `CONVERSION_FAILURE`.
### `enum class json::interface_misuse`
This enum is thrown if the interface was used in an incorrect manner, and may any of: `INCORRECT_TYPE`, `INDEX_OUT_OF_RANGE` or `NO_SUCH_KEY`.
