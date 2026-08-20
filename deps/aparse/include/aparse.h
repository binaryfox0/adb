/*
MIT License

Copyright (c) 2026 binaryfox0

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/* Source commit: 7de2e1ff70fb6fb7f60b850cde22a626bddd14f4 */

#ifndef APARSE_H
#define APARSE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Dynamic array container.
 *
 * Stores a contiguous sequence of fixed-size elements.
 * Elements are copied into an internal buffer and can be
 * accessed by index.
 */
typedef struct aparse_list
{
    /** Pointer to the underlying element storage. */
    void* ptr;

    /** Number of elements currently stored. */
    size_t size;

    /** Total number of elements that can be stored without resizing. */
    size_t capacity;

    /** Size of each element in bytes. */
    size_t itemsz;
} aparse_list;

/**
 * @brief Retrieves an element from the list.
 *
 * @param list Pointer to the list.
 * @param type Element type.
 * @param idx Zero-based element index.
 *
 * @return The element at @p idx cast to @p type.
 *
 * @warning No bounds checking is performed.
 */
#define aparse_list_get(list, type, idx) (*((type*)(list)->ptr + (idx)))

/**
 * @brief Initializes a list.
 *
 * Allocates storage for a dynamic array capable of holding
 * at least @p init_size elements.
 *
 * @param list List to initialize.
 * @param init_size Initial capacity in elements.
 * @param itemsz Size of each element in bytes.
 *
 * @return 1 on success, 0 on failure.
 */
int aparse_list_new(
        aparse_list* list,
        const size_t init_size,
        const size_t itemsz);

/**
 * @brief Resizes the list storage.
 *
 * Changes the list capacity to hold at least @p new_size elements.
 *
 * @param list List to resize.
 * @param new_size New capacity in elements.
 *
 * @return 1 on success, 0 on failure.
 */
int aparse_list_resize(
        aparse_list* list,
        const size_t new_size);

/**
 * @brief Appends an element to the list.
 *
 * Copies the item pointed to by @p data into the list.
 * The list may be automatically resized if additional
 * capacity is required.
 *
 * @param list Destination list.
 * @param data Pointer to the element to append.
 *
 * @return 1 on success, 0 on failure.
 */
int aparse_list_add(
        aparse_list* list,
        const void* data);

/**
 * @brief Releases all resources owned by the list.
 *
 * Frees the list's storage and resets its fields.
 *
 * @param list List to free.
 */
void aparse_list_free(aparse_list* list);

/** @cond HIDDEN */

#if defined(_MSC_VER)
#   define APARSE__INLINE __forceinline
#else
#   define APARSE__INLINE static inline __attribute__((always_inline))
#endif

#if !defined(APARSE_STDC_COMPLIANT) || \
    (defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL == 0) \
        || defined(__GNUC__) || defined(__clang__)

#   define APARSE__CAT_IMPL(a, b) a##b
#   define APARSE__CAT(a, b) APARSE__CAT_IMPL(a, b)

#   define APARSE__COUNT_ARGS2( \
        _0, _1, _2, _3, _4, _5, _6, _7, _8, \
        _9, _10,_11,_12,_13,_14,_15,_16,N,...) N
#   define APARSE__COUNT_ARGS1(...) \
        APARSE__COUNT_ARGS2(dummy, ##__VA_ARGS__, \
            16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#   define APARSE__COUNT_ARGS(...) \
        APARSE__COUNT_ARGS1(__VA_ARGS__)

#   define APARSE__MAP_0(...)           0
#   define APARSE__MAP_1(m, a, x)       m(a, x)
#   define APARSE__MAP_2(m, a, x, ...)  m(a, x), APARSE__MAP_1(m, a, __VA_ARGS__)
#   define APARSE__MAP_3(m, a, x, ...)  m(a, x), APARSE__MAP_2(m, a, __VA_ARGS__)
#   define APARSE__MAP_4(m, a, x, ...)  m(a, x), APARSE__MAP_3(m, a, __VA_ARGS__)
#   define APARSE__MAP_5(m, a, x, ...)  m(a, x), APARSE__MAP_4(m, a, __VA_ARGS__)
#   define APARSE__MAP_6(m, a, x, ...)  m(a, x), APARSE__MAP_5(m, a, __VA_ARGS__)
#   define APARSE__MAP_7(m, a, x, ...)  m(a, x), APARSE__MAP_6(m, a, __VA_ARGS__)
#   define APARSE__MAP_8(m, a, x, ...)  m(a, x), APARSE__MAP_7(m, a, __VA_ARGS__)
#   define APARSE__MAP_9(m, a, x, ...)  m(a, x), APARSE__MAP_8(m, a, __VA_ARGS__)
#   define APARSE__MAP_10(m, a, x, ...) m(a, x), APARSE__MAP_9(m, a, __VA_ARGS__)
#   define APARSE__MAP_11(m, a, x, ...) m(a, x), APARSE__MAP_10(m, a, __VA_ARGS__)
#   define APARSE__MAP_12(m, a, x, ...) m(a, x), APARSE__MAP_11(m, a, __VA_ARGS__)
#   define APARSE__MAP_13(m, a, x, ...) m(a, x), APARSE__MAP_12(m, a, __VA_ARGS__)
#   define APARSE__MAP_14(m, a, x, ...) m(a, x), APARSE__MAP_13(m, a, __VA_ARGS__)
#   define APARSE__MAP_15(m, a, x, ...) m(a, x), APARSE__MAP_14(m, a, __VA_ARGS__)
#   define APARSE__MAP_16(m, a, x, ...) m(a, x), APARSE__MAP_15(m, a, __VA_ARGS__)

#   define APARSE__OFFSETOF_AND_SIZEOF(s, m) offsetof(s, m), sizeof(((s*)0)->m )
#   define APARSE__OFFSETOFS(s, ...) { APARSE__EXPAND(APARSE__MAP(APARSE__OFFSETOF_AND_SIZEOF, s, __VA_ARGS__)) }

#   define APARSE__EXPAND(...) __VA_ARGS__
#   define APARSE__MAP(m, a, ...) \
    APARSE__CAT(APARSE__MAP_, APARSE__COUNT_ARGS(__VA_ARGS__))(m, a, __VA_ARGS__)

#   define APARSE__FIRST_ARG(a, ...) a
#   define APARSE__NOT_FIRST_ARG(a, ...) __VA_ARGS__


/** @endcond */

/**
 * @brief Create a subparser argument entry for @ref aparse_arg_s.
 *
 * This macro wraps `aparse_arg_subparser_impl` and automatically generates
 * the `data_layout` array for the specified members of `data_struct`.
 *
 * @param name        The argument name (string).
 * @param subargs     Pointer to subparser argument array.
 * @param handle      Function pointer to handler called after parsing.
 * @param buffer      Pointer to user-defined buffer described by the layout
 * @param size        The size of the buffer 
 * @param help        Help string describing this subparser.
 * @param data_struct Struct type containing members to map.
 * @param ...         List of member names of `data_struct` to include in the layout.
 *
 * @note Uses compiler-specific variadic macro expansions; some older compilers
 *       may not support it correctly
 *
 * @example
 * @code{.c}
 * struct config {
 *     int port;
 *     char *name;
 * };
 * aparse_arg_subparser(
 *          "config", 
 *          config_subargs, 
 *          handle_config, 
 *          NULL,
 *          0,
 *          "Config subparser", 
 *          config, port, name);
 * @endcode
 */
#   define aparse_arg_subparser( \
        name, \
        subargs, \
        handle, \
        buffer, \
        size, \
        help, \
        ...) \
       aparse_arg_subparser_impl( \
               (name), \
               (subargs), \
               (handle), \
               (buffer), \
               (size), \
               (help), \
               (size_t[]) \
                    APARSE__OFFSETOFS( \
                        APARSE__FIRST_ARG(__VA_ARGS__), \
                        APARSE__NOT_FIRST_ARG(__VA_ARGS__)), \
                APARSE__COUNT_ARGS(APARSE__NOT_FIRST_ARG(__VA_ARGS__)) \
        )
#endif

/** @} */ // end of aparse_macros

/** @cond HIDDEN */
#ifndef APARSE_NO_ANSI_ESCAPE
#    ifdef _WIN32
#       include <sdkddkver.h>
#       if defined(NTDDI_VERSION) && (NTDDI_VERSION >= NTDDI_WIN10_TH2)
#           define APARSE__ANSI_ESCAPE(x) x
#       else
#           define APARSE__ANSI_ESCAPE(x)
#       endif
#    else
#       define APARSE__ANSI_ESCAPE(x) x
#    endif
#endif

#define APARSE__DEBUG_LABEL \
    APARSE__ANSI_ESCAPE("\x1b[1;36m") "debug" APARSE__ANSI_ESCAPE("\x1b[0m")
#define APARSE__INFO_LABEL  \
    APARSE__ANSI_ESCAPE("\x1b[1;34m") "info"  APARSE__ANSI_ESCAPE("\x1b[0m")
#define APARSE__WARN_LABEL  \
    APARSE__ANSI_ESCAPE("\x1b[1;33m") "warn"  APARSE__ANSI_ESCAPE("\x1b[0m") 
#define APARSE__ERROR_LABEL \
    APARSE__ANSI_ESCAPE("\x1b[1;31m") "error" APARSE__ANSI_ESCAPE("\x1b[0m") 

#if defined(__GNUC__) || defined(__clang__)
#   define APARSE__PRINTF(fmt_index, arg_index) \
        __attribute__((format(printf, fmt_index, arg_index)))
#   define APARSE__PRINTF_FMT
#elif defined(_MSC_VER)
#   define APARSE__PRINTF(fmt_index, arg_index)
#   define APARSE__PRINTF_FMT _Printf_format_string_
#else
#   define APARSE__PRINTF(fmt_index, arg_index)
#   define APARSE__PRINTF_FMT
#endif
/** @endcond */

/**
 * @brief Print informational message to stderr, with color if supported
 */
#define aparse_prog_debug(...) \
    aparse_log(aparse_progname, APARSE__DEBUG_LABEL, __VA_ARGS__)

/**
 * @brief Print informational message to stderr, with color if supported
 */
#define aparse_prog_info(...) \
    aparse_log(aparse_progname, APARSE__INFO_LABEL, __VA_ARGS__)

/**
 * @brief Print warning message to stderr, with color if supported
 */
#define aparse_prog_warn(...) \
    aparse_log(aparse_progname, APARSE__WARN_LABEL, __VA_ARGS__)

/**
 * @brief Print error message to stderr, with color if supported
 */
#define aparse_prog_error(...) \
    aparse_log(aparse_progname, APARSE__ERROR_LABEL, __VA_ARGS__)


#ifdef __cplusplus
extern "C" {
#endif

extern const char* aparse_progname;
APARSE__PRINTF(3, 4) void aparse_log(
        const char *source,
        const char *type,
        APARSE__PRINTF_FMT const char *fmt,
        ...);

/**
 * @enum aparse_arg_types
 * @brief Enumerates all possible argument types recognized by the parser.
 *
 * These values define how each @ref aparse_arg_s entry should be interpreted.
 * Some constants are combinations or modifiers of others (bitmask-style flags).
 *
 * @see aparse_arg_s
 */
typedef enum aparse_arg_types
{
    /**
     * @brief Sign flag mask for signed argument types.
     * Used internally to mark that an argument type is signed.
     */
    APARSE_ARG_TYPE_SIGNED_FLAGS = (1 << 7),

    /**
     * @brief Unknown or uninitialized argument type.
     */
    APARSE_ARG_TYPE_UNKNOWN = 0,

    /**
     * @brief String argument type.
     * Stores a pointer to a null-terminated string.
     */
    APARSE_ARG_TYPE_STRING,

    /**
     * @brief Boolean argument type.
     * Typically represents flags like `--verbose` or `--quiet`.
     */
    APARSE_ARG_TYPE_BOOL,

    /**
     * @brief Unsigned integer argument type.
     */
    APARSE_ARG_TYPE_UNSIGNED,

    /**
     * @brief Floating-point argument type.
     */
    APARSE_ARG_TYPE_FLOAT,

    /**
     * @brief Array argument type.
     * Indicates that the argument points to a list of values 
     * rather than a single one.
     */
    APARSE_ARG_TYPE_ARRAY = (1 << 3),

    /**
     * @brief Positional argument type.
     * Used for arguments that are identified by their position, 
     * not by an option name.
     */
    APARSE_ARG_TYPE_POSITIONAL = (1 << 4),

    /**
     * @brief Normal argument type.
     * Used for standard named options like `--file` or `-f`.
     */
    APARSE_ARG_TYPE_ARGUMENT   = (1 << 5),

    /**
     * @brief Subparser or subcommand argument type.
     * Equivalent to @ref APARSE_ARG_TYPE_POSITIONAL, 
     * but used for defining subcommands.
     */
    APARSE_ARG_TYPE_SUBPARSER  = APARSE_ARG_TYPE_POSITIONAL,

    /**
     * @brief Signed integer argument type.
     * Combines @ref APARSE_ARG_TYPE_UNSIGNED with @ref APARSE_ARG_TYPE_SIGNED_FLAGS.
     */
    APARSE_ARG_TYPE_SIGNED = APARSE_ARG_TYPE_UNSIGNED | APARSE_ARG_TYPE_SIGNED_FLAGS,

    /**
     * @brief Bitmask used to extract base argument type.
     * Can be applied to a type value to ignore modifier flags.
     */
    APARSE_ARG_TYPE_BITMASK = 0x7
} aparse_arg_types;

/**
 * @brief Describes a single argument, option, or subparser definition.
 */
typedef struct aparse_arg aparse_arg;

/**
 * @brief Handler function called after successful subparser parsing.
 * 
 * @param arg  Pointer to `aparse_arg` contain information about the caller
 * @param data Pointer to the data structure described by @ref data_layout.
 */
typedef void (*aparse_handler_t)(const aparse_arg *arg, void *data);

/**
 * @struct aparse_arg
 * @brief Describes a single argument, option, or subparser definition.
 *
 * The `aparse_arg` structure defines the metadata for each command-line
 * argument or subparser, including its option names, data type, storage
 * pointer, and parsing behavior.
 *
 * The meaning of certain members depends on the argument type
 * (see @ref aparse_arg_types).
 */
typedef struct aparse_arg
{
    /**
     * @brief The short option name (e.g., "-h") for optional arguments.
     *
     * - Ignored for positional arguments.  
     * - May be `NULL` if `longopt` is used instead.
     */
    const char* shortopt;

    /**
     * @brief The long option name (e.g., "--help") for optional arguments.
     *
     * - Required for positional arguments.  
     * - May be `NULL` if `shortopt` is present and sufficient.
     */
    const char* longopt;

    /**
     * @brief A help message describing the argument.
     *
     * The message will be displayed in help outputs generated by the parser.
     * Can be NULL
     */
    const char* help;

    /**
     * @brief The argument type.
     *
     * Indicates whether the argument is a string, integer, array, subparser, etc.
     * See @ref aparse_arg_types for all possible values.
     */
    aparse_arg_types type;

    /**
     * @brief Internal parser flags to track parsing process.
     *
     * Typically not set manually by the user.
     */
    uint8_t flags;

    /**
     * @brief Type-dependent size or count parameter.
     *
     * The meaning of this field depends on the argument type:
     *
     * **1. Normal arguments (`APARSE_ARG_TYPE_ARGUMENT`)**
     * - Defines the size (in bytes) of the destination variable pointed to by `ptr`.
     * - For string arguments (`APARSE_ARG_TYPE_STRING`):
     *   - If `size == 0`, the parser assigns `argv[index]` directly to `*ptr`
     *     (do not free it). The user must pass `const char**` as the target pointer.
     * - Recommended: use the `sizeof()` operator on the destination variable.
     *
     * **2. Array arguments (`APARSE_ARG_TYPE_ARRAY`)**
     * - Defines the number of elements in the destination array.
     *
     * **3. Subparser arguments (`APARSE_ARG_TYPE_SUBPARSER`)**
     * - Defines the number of offset-size pairs in @ref data_layout.
     */
    size_t size;
             
    /**
     * @brief Pointer to the variable where the parsed value will be stored.
     */
    void* ptr;

    /**
     * @brief Type-specific data fields.
     *
     * The union contains either:
     * - Members used for normal/array arguments (`ptr`, `element_size`), or
     * - Members used for subparser/subcommand definitions (`subargs`, `handler`, `data_layout`).
     */
    union {
       /**
         * @brief Argument-only fields (used when `type` include `APARSE_ARG_TYPE_ARGUMENT`).
         */
        struct {
            /**
             * @brief Desired size of the array arguments
             */
            size_t array_size;
            /**
             * @brief Size of each element for array arguments.
             */
            size_t element_size;
        };
        // For subparsers/subcommands
        struct {
            /**
             * @brief Array of subarguments used in the subcommand.
             */
            struct aparse_arg* subargs;

            /**
             * @brief Handler function called after successful subparser parsing.
             */
            aparse_handler_t handler;

            /**
             * @brief Array describing the data layout passed to the handler.
             *
             * Each pair in the array represents `{offset, size}` for a member in the
             * destination structure. Example:
             *
             * @code{.c}
             * struct config {
             *     int port;
             *     char *name;
             * };
             *
             * int layout[] = {
             *     offsetof(struct config, port), sizeof(int),
             *     offsetof(struct config, name), sizeof(char*)
             * };
             * @endcode
             */
            const size_t *data_layout;

            /**
             * @brief The size of \p data_layout
             */
            size_t layout_size;
        };
    };
} aparse_arg;

/**
 * @enum aparse_status
 * @brief Status codes reported to error callbacks.
 */
typedef enum aparse_status
{
    APARSE_STATUS_OK = 0,               /**< Parsing succeeded with no errors. */
    
    APARSE_STATUS_FAILURE,              /**< General parsing failure (unspecified). */
    APARSE_STATUS_UNKNOWN_ARGUMENT,     /**< Unrecognized command-line argument or option. */

    APARSE_STATUS_MISSING_VALUE,        /**< Options/Arrays expected multiple value, but none was provided */
    APARSE_STATUS_INVALID_VALUE,        /**< Value provided for the argument was invalid */
    APARSE_STATUS_OVERFLOW,             /**< Numeric value exceeds supported range */
    APARSE_STATUS_UNDERFLOW,            /**< Numeric value is below supported range */

    APARSE_STATUS_MISSING_POSITIONAL,   /**< Expected positional argument was not provided. */
    APARSE_STATUS_INVALID_SUBCOMMAND,   /**< Subcommand not found or unrecognized. */

    APARSE_STATUS_NULL_POINTER,         /**< A required pointer argument was NULL. */
    APARSE_STATUS_INVALID_TYPE,         /**< Argument type is invalid or mismatched. */
    APARSE_STATUS_INVALID_SIZE,         /**< Argument size is invalid for its type. */

    APARSE_STATUS_INVALID_LAYOUT,       /**< The layout is invalid */

    APARSE_STATUS_ALLOC_FAILURE,        /**< Memory allocation failed. */
    APARSE_STATUS_UNHANDLED,            /**< Unhandled type of argument. */
    APARSE_STATUS_TOO_DEEP,             /**< Parser nesting depth exceeded the limit */

    __APARSE_STATUS_ENUM_END__          /**< The marker for the end of aparse_status. THIS MUST BE AT THE END */
} aparse_status;

/**
 * @brief Opaque parsing context.
 *
 * Contains the internal state of the parser and may be passed to
 * callbacks for accessing parser-related information.
 */
typedef struct aparse_context aparse_context;

/**
 * @brief Error callback function type for reporting parsing issues.
 *
 * @param status     The error code (see ::aparse_status).
 * @param field1     Context-specific first data pointer.
 * @param field2     Context-specific secondary pointer (may be NULL).
 * @param userdata   The user-provided pointer from the parser context.
*
 * @note
 * The parser never frees or modifies the data passed through @p field1 or @p field2.
 * The callback should treat them as read-only.
 *
 * | Status code                        | field1 type            | field2 type            | Description                                       |
 * |------------------------------------|------------------------|------------------------|---------------------------------------------------|
 * | ::APARSE_STATUS_UNKNOWN_ARGUMENT   | `unknown_args`         | `NULL`                 | Unknown argument name.                            |
 * | ::APARSE_STATUS_MISSING_VALUE      | `current_arg`          | `expected_count`       | Option definition that requires a value.          |
 * | ::APARSE_STATUS_INVALID_VALUE      | `current_arg`          | `current_argv`         | Argument definition and invalid value string.     |
 * | ::APARSE_STATUS_OVERFLOW           | `current_arg`          | `current_argv`         | Numeric argument and overflowing value string.    |
 * | ::APARSE_STATUS_UNDERFLOW          | `current_arg`          | `current_argv`         | Numeric argument and underflowing value string.   |
 * | ::APARSE_STATUS_MISSING_POSITIONAL | `required_args`        | `NULL`                 | Missing positional argument definition.           |
 * | ::APARSE_STATUS_INVALID_SUBCOMMAND | `parser_subargs`       | `current_argv`         | Invalid subcommand name.                          |
 * | ::APARSE_STATUS_NULL_POINTER       | `current_arg`          | `NULL`                 | Invalid NULL pointer in user argument definition. |
 * | ::APARSE_STATUS_INVALID_TYPE       | `current_arg`          | `NULL`                 | Type mismatch in argument definition.             |
 * | ::APARSE_STATUS_INVALID_SIZE       | `current_arg`          | `arg_size`             | Invalid size field in argument definition.        |
 * | ::APARSE_STATUS_INVALID_LAYOUT     | `current_arg`          | `index`                | The layout was invalid at index                   |
 * | ::APARSE_STATUS_ALLOC_FAILURE      | `NULL`                 | `NULL`                 | Memory allocation failed inside parser.           |
 * | ::APARSE_STATUS_UNHANDLED          | `current_arg`          | `NULL`                 | An unhandled type of argument.                    |
 * | ::APARSE_STATUS_TOO_DEEP           | `NULL`                 | `NULL`                 | Parser nesting depth exceeded the limit           |
 *
 * - `const aparse_list* unknown_args  `: An aparse_list refer to a list of arguments. `unknown_args.ptr` should be converted into `aparse_arg*`
 * - `const aparse_arg*  current_arg   `: An aparse_arg* refer to the currently processed argument.
 * - `const int*         expected_count`: The count of expected argv of `current_arg`
 * - `const char*        cargv         `: The current argv is currently being processed
 * - `const aparse_list* required_args `: An aparse_list refer to a list of required arguments. `required_args.ptr` should be converted into `aparse_arg*`
 * - `const int*         size          `: The invalid size of `current_arg`. It can be `current_arg.size` or `current_arg.element_size`
 * - `const int*         index         `: The base index of current entry inside `current_arg.data_layout`
 */
typedef void (*aparse_error_callback)(
        const aparse_context *ctx,
        const aparse_status status, 
        const void* field1, 
        const void* field2, 
        void *userdata);

/**
 * @brief Create an option argument (flag with value).
 *
 * @param shortopt   Short option string (e.g., "-o"), may be NULL.
 * @param longopt    Long option string (e.g., "--output"), may be NULL.
 * @param dest       Pointer to destination variable.
 * @param size       Size of destination variable in bytes.
 * @param type       Value type (see ::aparse_arg_types).
 * @param help       Help string (optional).
 *
 * @return Constructed ::aparse_arg definition.
 */
APARSE__INLINE aparse_arg aparse_arg_option(
        const char* shortopt, 
        const char* longopt, 
        void* dest, 
        const size_t size, 
        const aparse_arg_types type, 
        const char* help) 
{
    return (aparse_arg){
        .shortopt = shortopt, 
        .longopt = longopt,
        .type = type | 
            APARSE_ARG_TYPE_ARGUMENT, 
        .ptr = dest, 
        .size = size, 
        .help = help,
    };
}

/**
 * @brief Create a numeric positional argument.
 *
 * Defines a positional argument that expects a numeric value, such as an integer or float.
 * Typically used for command-line arguments that represent numbers, counts, or indices.
 *
 * @param name  Argument name (used for help and matching).
 * @param dest  Pointer to destination variable where the parsed number is stored.
 * @param size  Size of the destination variable in bytes.
 * @param type  Data type of the number (e.g., ::APARSE_ARG_TYPE_SIGNED, ::APARSE_ARG_TYPE_FLOAT).
 * @param help  Optional help string to describe this argument.
 *
 * @return A fully constructed ::aparse_arg definition for numeric positional arguments.
 */
APARSE__INLINE aparse_arg aparse_arg_number(
        const char* name, 
        void* dest, 
        const size_t size, 
        const aparse_arg_types type, 
        const char* help) 
{
    return (aparse_arg){
        .longopt = name, 
        .ptr = dest, 
        .size = size, 
        .help = help,
        .type = type | 
            APARSE_ARG_TYPE_POSITIONAL | 
            APARSE_ARG_TYPE_ARGUMENT
    };    
}

/**
 * @brief Create a string positional argument.
 *
 * Defines a positional argument that accepts a single string value.
 * Typically used for filenames, paths, or text parameters.
 *
 * @param name  Argument name (used for help and matching).
 * @param dest  Pointer to destination buffer where the string will be stored.
 * @param size  Size of the destination buffer in bytes.
 * @param help  Optional help string to describe this argument.
 *
 * @return A fully constructed ::aparse_arg definition for string positional arguments.
 *
 * @note If @p size is 0, `dest` will be assigned with the string of argument (`const char*`)
 */
APARSE__INLINE aparse_arg aparse_arg_string(
        const char* name, 
        void* dest, 
        const size_t size, 
        const char* help) 
{
    return (aparse_arg) {
        .longopt = name, 
        .ptr = dest, 
        .size = size,
        .help = help,
        .type = APARSE_ARG_TYPE_STRING |
            APARSE_ARG_TYPE_POSITIONAL |
            APARSE_ARG_TYPE_ARGUMENT
    };
}

/**
 * @brief Create a subparser definition.
 *
 * Defines a subparser (subcommand) entry with its own argument list and handler function.
 * Useful for CLI tools with multiple modes or subcommands, e.g.:
 * @code
 * mytool build ...
 * mytool test ...
 * @endcode
 *
 * @param name           Subcommand name (e.g. "build", "test").
 * @param subargs        Argument table for the subparser.
 * @param handle         Function pointer to the subcommand handler.
 * @param buffer         Pointer to a buffer described by data_layout
 * @param size           Size of the buffer
 * @param help           Optional help string for this subparser.
 * @param data_layout    Pointer to custom data layout.
 * @param layout_size    Size of the data layout array.
 *
 * @return A fully constructed ::aparse_arg definition representing the subparser.
 *
 * @note The parser core ignores this type during matching, but it is used
 *       by help and usage generators to represent subcommands.
 * @note If `ptr` is not provided, aparse will automatically allocate a buffer based on the provided layout, enabling seamless usage without manual memory setup.
 * @attention Generally recommended to use ::aparse_arg_subparser
 */
APARSE__INLINE aparse_arg aparse_arg_subparser_impl(
        const char* name,
        aparse_arg* subargs, 
        const aparse_handler_t handler,
        void *buffer, 
        const size_t size,
        const char* help, 
        const size_t *data_layout, 
        const size_t layout_size
) {
    return (aparse_arg) {
        .longopt = name, 
        .subargs = subargs, 
        .handler = handler,
        .data_layout = data_layout, 
        .layout_size = layout_size,
        .ptr = buffer, 
        .size = size,
        .help = help,
        .type = APARSE_ARG_TYPE_SUBPARSER 
    };
}

/**
 * @brief Create a root parser definition.
 *
 * Defines the top-level parser node that groups subparsers together.
 * This is used to construct hierarchical command structures.
 *
 * @param name        Name of the root parser (typically the program name).
 * @param subparsers  Array of subparser definitions (terminated with ::aparse_arg_end_marker).
 *
 * @return A constructed ::aparse_arg definition representing the root parser.
 */
APARSE__INLINE aparse_arg aparse_arg_parser(
        const char* name, 
        aparse_arg* subparsers) 
{
    return (aparse_arg){
        .longopt = name, 
        .subargs = subparsers, 
        .type = APARSE_ARG_TYPE_POSITIONAL
    };
}

/**
 * @brief Create an array argument definition.
 *
 * Defines a positional argument that stores multiple values (array behavior).
 * Useful for arguments that can appear multiple times or accept lists of items.
 *
 * @param name           Argument name.
 * @param dest           Pointer to the array where parsed values will be stored.
 * @param array_size     Total size of the destination array in bytes.
 * @param type           Element type (see ::aparse_arg_types).
 * @param element_size   Size of each array element in bytes (0 for pointer arrays).
 * @param help           Optional help string.
 *
 * @return A fully constructed ::aparse_arg definition representing an array argument.
 *
 * @note If @p element_size is 0, the parser assumes an array of pointers (`char*`).
 */
APARSE__INLINE aparse_arg aparse_arg_array(
        const char* name, 
        void* dest, 
        const size_t size, 
        const size_t array_size, 
        const aparse_arg_types type, 
        const size_t element_size, 
        const char* help) {
    return (aparse_arg){
        .longopt = name, .ptr = dest, .size = size,
        .array_size = array_size / (element_size == 0 ? sizeof(char*) : element_size),
        .type = 
            APARSE_ARG_TYPE_ARGUMENT | 
            APARSE_ARG_TYPE_ARRAY | 
            APARSE_ARG_TYPE_POSITIONAL | 
            type,
        .help = help, .element_size = element_size
    };
}

/**
 * @brief End marker for argument definition tables.
 *
 * Used as a sentinel value to mark the end of an ::aparse_arg array.
 * The parser stops reading arguments when it encounters this marker.
 *
 * @code{.c}
 * aparse_arg args[] = {
 *     aparse_arg_string("file", &file, sizeof(file), "Input file"),
 *     aparse_arg_number("count", &count, sizeof(count), APARSE_ARG_TYPE_INT, "Number of items"),
 *     aparse_arg_end_marker
 * };
 * @endcode
 */
#define aparse_arg_end_marker (aparse_arg){0}

/**
 * @brief Check if the current aparse_arg was an end marker
 *
 * @param arg The aparse_arg to check
 *
 * @note This will only check for `longopt` and `shortopt`, all the remaining
 * information will be discarded
 */
static inline bool aparse_arg_nend(const aparse_arg* arg) {
    return arg->longopt != 0 || arg->shortopt != 0;
}

/**
 * @brief Parse command-line arguments.
 *
 * Parses command-line input (`argc` / `argv`) according to a provided
 * argument table. Supports short and long options, positional arguments,
 * and subcommands.
 *
 * @param argc              Argument count (from `main`).
 * @param argv              Argument vector (from `main`).
 * @param args              Argument definition table, terminated with ::aparse_arg_end_marker.
 * @param dispatch_list_out Optional output for the list of dispatched function
 * @param program_desc      Optional program description for `--help` output (may be NULL).
 *
 * @return One of the ::aparse_status codes, typically ::APARSE_STATUS_OK on success.
 *
 * @note Errors and warnings can be intercepted using ::aparse_set_error_callback.
 * @note If `dispatch_list == NULL`, dispatched function will be executed immedieately after parsing complete
 */
aparse_status aparse_parse(
        const int argc, 
        char* const * argv, 
        aparse_arg* args, 
        aparse_list* dispatch_list_out, 
        const char* program_desc
);

/**
 * @brief Dispatch all queued handle
 *
 * Dispatch all handle with their respective constructed payload, then
 * also freeing any resources related to payload
 *
 * @param dispatch_list The list of dispatched functions
 */
extern void aparse_dispatch_all(aparse_list* dispatch_list);

/**
 * @brief Check for the handle inside dispatch list
 *
 * Check if the handle inside dispatch list was existed with given name,
 * normally it will be compared against `aparse_arg.longopt`
 *
 * @param name Name of dispatch handle to find
 *
 * @return If it wasn't existed in `dispatch_list`, return 1, otherwise return 0
 */
extern int aparse_dispatch_contain(const aparse_list* dispatch_list, const char* name);

/**
 * @brief Free a dispatch list without executing handlers
 *
 * Releases all resources associated with the dispatch list and its queued
 * handlers without invoking any handler functions. Any constructed payloads
 * stored in the list are freed.
 *
 * This function is typically used when argument parsing fails or when
 * execution of dispatched handlers is intentionally skipped.
 *
 * @param dispatch_list List of queued dispatch handlers to be freed
 */
extern void aparse_dispatch_free(aparse_list* dispatch_list);

/**
 * @brief Set a global error callback for parser events.
 *
 * Registers a callback function that is called whenever the parser
 * encounters an error or warning during parsing.
 *
 * @param cb        Pointer to a callback function of type ::aparse_error_callback.
 * @param userdata  User-defined pointer passed to the callback on each invocation.
 *
 * @note Passing `NULL` as @p cb using the library default callback.
 */
void aparse_set_error_callback(
        const aparse_error_callback cb, 
        void* userdata);

/**
 * @brief Returns a human-readable error message forstatus code.
 * @param status The status code.
 * @return A pointer to a constant null-terminated string if existed,
 *         otherwise a "Unknown error" string
 *
 * @warning The returned pointer must not be modified or freed.
 */
const char* aparse_error_msg(const aparse_status status);

#ifdef __cplusplus 
}
#endif



#ifdef APARSE_IMPLEMENTATION

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
#include <math.h>

/* -----------/home/binaryfox0/proj/aparse/src/aparse.c BEGIN----------- */

#ifdef _WIN32
#   include <windows.h>
#else
#   include <unistd.h>
#   include <sys/ioctl.h>
#endif

#define APARSE__SPACE_PER_INDENT 2 // indent/space
#define MAX_ARG_STR 19
#define APARSE__MAX_DEPTH 16

// aparse_arg flags
// optional  | has_equal   APARSE_ARG_EQUAL_VAL
// optional  | short_match APARSE_ARG_SHORT_MATCH
// reserved  | 
// reserved  | 
// reserved  | 
// reserved  | 
// universal | processed   APARSE_ARG_PROCESSED

#define APARSE_IS_LE (*(unsigned char *)&(uint16_t){1})
#define APARSE_ALLOC_SIZE 5

#define APARSE__MIN(a, b) ((a < b) ? (a) : (b))

#define aparse__lib__debug(...) \
    aparse_log("aparse", APARSE__DEBUG_LABEL, __VA_ARGS__)
#define aparse__lib__info(...) \
    aparse_log("aparse", APARSE__INFO_LABEL, __VA_ARGS__)
#define aparse__lib__warn(...) \
    aparse_log("aparse", APARSE__WARN_LABEL, __VA_ARGS__)
#define aparse__lib__error(...) \
    aparse_log("aparse", APARSE__ERROR_LABEL, __VA_ARGS__)

#define aparse__raise_fatal(ctx, type, field1, field2) \
    { \
        aparse__err_callback((ctx), (type), (field1), (field2), aparse__err_userdata); \
        return APARSE_STATUS_FAILURE; \
    }

#define aparse__raise_nonfatal(ctx, type, field1, field2) \
        aparse__err_callback((ctx), (type), (field1), (field2), aparse__err_userdata);

#define aparse__foreach(child, parent) \
    for(aparse_arg *child = parent->subargs; aparse_arg_nend(child); child++)
#define aparse__tillend(element, start) \
    for(aparse_arg *element = start; aparse_arg_nend(element); element++)

typedef enum {
    APARSE__ARG_EQUAL_VAL   = (1 << 0),
    APARSE__ARG_SHORT_MATCH = (1 << 1),
    APARSE__ARG_PROCESSED   = (1 << 7)
} aparse_arg_state_t;


typedef struct {
    aparse_arg* args;
    void* payload;
} aparse__dispatch_t;

typedef struct aparse_context
{
    int idx;
    aparse_list *unknown;
    aparse_list *dispatch;
    int layer_idx;

    aparse_arg *stack[APARSE__MAX_DEPTH];
    int stack_top;
} aparse__context_t;

APARSE__INLINE bool aparse__is_positional(
        const aparse_arg* arg) {
    return arg->type & APARSE_ARG_TYPE_POSITIONAL;
}

APARSE__INLINE bool aparse__is_argument(
        const aparse_arg* arg) {
    return arg->type & APARSE_ARG_TYPE_ARGUMENT;
}
static inline bool aparse__type_cmp(
        const aparse_arg* arg, 
        const aparse_arg_types type) {
    return (arg->type & APARSE_ARG_TYPE_BITMASK) == type;
}

const char* aparse_progname = 0;
static const char *aparse__desc = 0;

static const aparse_arg aparse__help_arg = 
{ 
    .shortopt = "-h", 
    .longopt = "--help", 
    .help = "show this help message and exit", 
    .type = APARSE_ARG_TYPE_BOOL 
};
static aparse_error_callback aparse__err_callback = 0;
static void* aparse__err_userdata = 0;

// Forward declaration
static aparse_status aparse__parse_impl(
        const int argc, 
        char * const * argv, 
        aparse_arg* args, 
        aparse__context_t* ctx
);

// Processing each type of argument
static aparse_status aparse__process_argument(
        const char* argv, 
        const aparse_arg* arg,
        aparse__context_t *ctx);

static aparse_status aparse__process_parser(
        const int argc, 
        const char* cargv, 
        char* const* argv, 
        aparse_arg* arg, 
        aparse__context_t* ctx
);

static aparse_status aparse_process_optional(
        const int argc, 
        char* const* argv, 
        aparse_arg* arg,
        aparse__context_t *ctx
);
// Processing each data type
static aparse_status aparse__process_float(
        const char* argv, 
        const aparse_arg* arg);

static aparse_status aparse__process_array(
        const int argc, 
        char* const* argv, 
        aparse_arg* arg, 
        aparse__context_t *ctx);

// For aparse_arg is subparsers and have proper data_layout & layout_size
static size_t aparse__eval_size(
        const aparse_arg* arg);

static bool aparse__verify_layout(
        const aparse_arg *arg, 
        int *invalid_idx);

static int aparse__fill_args_dest(
        const aparse_arg* arg, 
        uint8_t *buffer);

static void aparse__destroy_payload(
        const aparse_arg* args, 
        uint8_t *payload);
// Failure handling
static aparse_status aparse__check_missing(
        aparse_context* ctx,
        aparse_arg* args);

static void aparse__default_errcb(
        const aparse_context* ctx,
        const aparse_status status, 
        const void* field1, 
        const void* field2, 
        void* userdata);

// Help-related functions
static void aparse__print_help(
        aparse_arg* main_args,
        aparse__context_t *ctx);

static void aparse__print_wrapped(
        const char *text, 
        int start_col, 
        int term_width);

static void aparse__print_help_tag(
        const aparse_arg* arg, 
        const int indent
);

static void aparse__print_pos_help(
        aparse_arg* args);

static void aparse__print_subcmds(
        const aparse_arg* args);

static void aparse__print_usage(
        const aparse_context *ctx);

/* Miscellaneous functions */
static void aparse__reset_state(
        aparse_arg *args);

static aparse_arg* aparse__argv_match(
        const char* argv, 
        aparse_arg* args);

static const char* aparse__get_exename(
        const char* argv0);
static size_t aparse__option_value_index(const char* opt);

static int aparse__get_term_width(void);

void aparse_log(
        const char *source,
        const char *type,
        const char *fmt,
        ...)
{
    va_list va;
    if(source)
    {
        fputs(source, stderr);
        fputs(": ", stderr);
    }

    if(type)
    {
        fputs(type, stderr);
        fputs(": ", stderr);
    }

    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    fputc('\n', stderr);
}

aparse_status aparse_parse(
        const int argc, 
        char* const * argv,
        aparse_arg* args, 
        aparse_list* dispatch_list_out, 
        const char* program_desc)
{
    aparse_status ret = APARSE_STATUS_OK;
    aparse__context_t ctx = {0};
    aparse_list unknown_list = {0}; // char*
    aparse_list dispatch_list = {0}; // aparse__dispatch_entry_t*

    if(!argv || argc < 1)
        return APARSE_STATUS_FAILURE;
    aparse_progname = aparse__get_exename(argv[0]);
    aparse__desc = program_desc;

    if(!args)
        return APARSE_STATUS_OK;

    unknown_list.itemsz = sizeof(const char*);
    dispatch_list.itemsz = sizeof(aparse__dispatch_t);

    ctx.idx = 1;
    ctx.unknown = &unknown_list;
    ctx.dispatch = &dispatch_list;
    
    if(!aparse__err_callback)
        aparse_set_error_callback(NULL, NULL);

    aparse__reset_state(args);
    ret = aparse__parse_impl(argc, argv, args, &ctx);
    if(ret == APARSE_STATUS_OK)
        ret = aparse__check_missing(&ctx, args);
    
    if(ret == APARSE_STATUS_OK && unknown_list.size > 0)
    {
        aparse__raise_nonfatal(&ctx, APARSE_STATUS_UNKNOWN_ARGUMENT, \
                &unknown_list, NULL);
        ret = APARSE_STATUS_FAILURE;
    }
    aparse_list_free(&unknown_list);

    if(ret == APARSE_STATUS_OK)
    {
        if(dispatch_list_out)
            *dispatch_list_out = dispatch_list;
        else
            aparse_dispatch_all(&dispatch_list);
    }

    return ret;
}

void aparse_dispatch_all(
        aparse_list* dispatch_list)
{
    if(!dispatch_list || !dispatch_list->ptr || dispatch_list->size < 1)
        return;
    if(dispatch_list->itemsz != sizeof(aparse__dispatch_t))
        return;

    for(size_t i = 0; i < dispatch_list->size; i++)
    {
        aparse__dispatch_t *entry = 
            &aparse_list_get(dispatch_list, aparse__dispatch_t, i);

        entry->args->handler(entry->args, entry->payload);
        aparse__destroy_payload(entry->args, entry->payload);
    }
    aparse_list_free(dispatch_list);
}

int aparse_dispatch_contain(
        const aparse_list* dispatch_list, 
        const char* name)
{
    if(
            !dispatch_list || 
            !dispatch_list->ptr || 
            dispatch_list->size < 1 || 
            !name || 
            !*name
    ) return 0;
    aparse__dispatch_t* list = dispatch_list->ptr;
    for(size_t i = 0; i < dispatch_list->size; i++)
    {
        if(!list[i].args->longopt)
            continue;
        if(!strcmp(list[i].args->longopt, name))
            return 1;
    }
    return 0;
}

void aparse_dispatch_free(
        aparse_list* dispatch_list)
{
    if(!dispatch_list || !dispatch_list->ptr || dispatch_list->size < 1)
        return;
    aparse__dispatch_t* list = dispatch_list->ptr;
    for(size_t i = 0; i < dispatch_list->size; i++)
        if(list[i].payload)
            free(list[i].payload);
}

void aparse_set_error_callback(const aparse_error_callback cb, void* userdata)
{
    if(!cb)
    {
        aparse__err_callback = aparse__default_errcb;
        aparse__err_userdata = NULL;
        return;
    }
    
    aparse__err_callback = cb;
    aparse__err_userdata = userdata;
}

const char* aparse_error_msg(const aparse_status status)
{
    static const char* error_msg[] = 
    {
        [APARSE_STATUS_OK]                  = "Parsing succeeded with no errors.",
        [APARSE_STATUS_FAILURE]             = "General parsing failure (unspecified).",
        [APARSE_STATUS_UNKNOWN_ARGUMENT]    = "Unrecognized command-line argument or option.",
        [APARSE_STATUS_MISSING_VALUE]       = "Options/Arrays expected multiple value, but none was provided.",
        [APARSE_STATUS_INVALID_VALUE]       = "Value provided for the argument was invalid.",
        [APARSE_STATUS_OVERFLOW]            = "Numeric value exceeds supported range.",
        [APARSE_STATUS_UNDERFLOW]           = "Numeric value is below supported range.",
        [APARSE_STATUS_MISSING_POSITIONAL]  = "Expected positional argument was not provided.",
        [APARSE_STATUS_INVALID_SUBCOMMAND]  = "Subcommand not found or unrecognized.",
        [APARSE_STATUS_NULL_POINTER]        = "A required pointer argument was NULL.",
        [APARSE_STATUS_INVALID_TYPE]        = "Argument type is invalid or mismatched.",
        [APARSE_STATUS_INVALID_SIZE]        = "Argument size is invalid for its type.",
        [APARSE_STATUS_INVALID_LAYOUT]      = "The given data layout for subcommand is invalid",
        [APARSE_STATUS_ALLOC_FAILURE]       = "Memory allocation failed.",
        [APARSE_STATUS_UNHANDLED]           = "Unhandled type of argument."
    };
    if(status < 0 && status >= __APARSE_STATUS_ENUM_END__)
        return "Unknown error";
    return error_msg[status];
}

// --------------------------------------- PRIVATE ---------------------------------------
static aparse_status aparse__parse_impl(
        const int argc, 
        char* const * argv, 
        aparse_arg* args, 
        aparse__context_t* ctx)
{
    int *idx = &ctx->idx;
    if(!args)
        return APARSE_STATUS_OK;

    if(ctx->stack_top >= APARSE__MAX_DEPTH)
        return APARSE_STATUS_TOO_DEEP;
    
    ctx->stack[ctx->stack_top++] = args;
    while (*idx < argc) {
        const char* cargv = argv[*idx];
        (*idx)++;

        aparse_arg* ptr = aparse__argv_match(cargv, args);
        if(ptr) {
            if(aparse__is_positional(ptr)) 
            {
                ptr->flags |= APARSE__ARG_PROCESSED;
                if(aparse__is_argument(ptr)) 
                {
                    aparse_status status = APARSE_STATUS_OK;
                    if(ptr->type & APARSE_ARG_TYPE_ARRAY)
                    {
                        status = aparse__process_array(
                                argc, 
                                argv, 
                                ptr, 
                                ctx);
                    } else {
                        status = aparse__process_argument(
                                cargv, 
                                ptr, 
                                ctx);
                    }

                    if(status != APARSE_STATUS_OK)
                        return APARSE_STATUS_FAILURE;
                } else  {
                    if(aparse__process_parser(argc, cargv, argv, ptr, 
                                ctx) != APARSE_STATUS_OK)
                        return APARSE_STATUS_FAILURE;
                    ctx->stack_top--;
                }
            } else {
                if(ptr->shortopt != aparse__help_arg.shortopt) {
                    if(aparse_process_optional(
                                argc, 
                                argv, 
                                ptr, 
                                ctx) != APARSE_STATUS_OK)
                        return APARSE_STATUS_FAILURE;
                } else {
                    aparse__print_help(args, ctx);
                    return APARSE_STATUS_FAILURE;
                }
            }
        } else {
            if (ctx->stack_top > 1) {
                // allow option to be processed again
                (*idx)--; 
                break;
            }

            if (!aparse_list_add(ctx->unknown, &cargv)) 
            {
                aparse__raise_fatal(ctx, APARSE_STATUS_ALLOC_FAILURE, 
                        NULL, NULL);
            }
        }
    }

    return APARSE_STATUS_OK;
}

static aparse_status aparse__process_argument(
        const char* argv, 
        const aparse_arg *arg,
        aparse__context_t *ctx) 
{
    if (!arg->ptr)
    {
        aparse__raise_nonfatal(ctx, APARSE_STATUS_NULL_POINTER, arg, NULL);
        return APARSE_STATUS_OK; // continue
    }
    if (arg->size <= 0 && !(arg->type & APARSE_ARG_TYPE_STRING))
        aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_SIZE, arg, &arg->size);

    switch(arg->type & APARSE_ARG_TYPE_BITMASK)
    {
        case APARSE_ARG_TYPE_STRING:
        {
            size_t len = strlen(argv) + 1;

            if (arg->size == 0) 
            {
                const char** dst = (const char**)arg->ptr;
                *dst = argv;
            } else {
                size_t n = APARSE__MIN(arg->size - 1, len);
                memcpy(arg->ptr, argv, n);
                ((char*)arg->ptr)[n] = '\0';
            }
            break;
        }
        case APARSE_ARG_TYPE_UNSIGNED:
        {
            bool have_sign = arg->type & APARSE_ARG_TYPE_SIGNED_FLAGS;
            if(arg->size > sizeof(uint64_t))
                aparse__raise_fatal(ctx, APARSE_STATUS_UNHANDLED, arg, 0);

            // Determine base
            int base = 10;
            const char* p = argv;
            bool is_negative = false;

            if (*p == '-' || *p == '+') {
                if (*p == '-') is_negative = true;
                p++;
            }
            if(is_negative && !have_sign)
                aparse__raise_fatal(ctx, APARSE_STATUS_UNDERFLOW, arg, 0);

            if (p[0] == '0') {
                switch(tolower(p[1]))
                {
                    case 'x': base = 16; p += 2; break;
                    case 'b': base = 2 ; p += 2; break;
                    case 'o': base = 8 ; p += 2; break;
                    default:  base = 8;  p++; break;
                }
            }

            errno = 0;
            char* endptr = 0;
            uint64_t num = strtoull(p, &endptr, base);
            
            if(*endptr)
                aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_VALUE, arg, argv);
     
            // Overflow/Underflow check No.1 (for 8 bytes)
            if(errno == ERANGE)
            {
                if(num == ULLONG_MAX || (int64_t)num == LLONG_MAX)
                    aparse__raise_fatal(ctx, APARSE_STATUS_OVERFLOW, arg, argv);
                if((int64_t)num == LLONG_MIN)
                    aparse__raise_fatal(ctx, APARSE_STATUS_UNDERFLOW, arg, argv);
            } else {
                // Overflow/Underflow check No.2
                size_t bits = arg->size * 8;
                if (have_sign) {
                    if(is_negative)
                    {
                        if(num > (uint64_t)INT64_MAX + 1ULL)
                            aparse__raise_fatal(ctx, APARSE_STATUS_UNDERFLOW, arg, argv);
                        if(num == (uint64_t)INT64_MAX + 1ULL)
                            num = (uint64_t)INT64_MIN;
                        else
                            num = (uint64_t) -(int64_t)num;
                    } else {
                        int64_t min_val = 0;
                        int64_t max_val = 0;

                        if(num > (uint64_t)INT64_MAX)
                            aparse__raise_fatal(ctx, APARSE_STATUS_OVERFLOW, arg, argv);
                        min_val = bits == 64 ? INT64_MIN : -(1LL << (bits - 1));
                        max_val = bits == 64 ? INT64_MAX :  (1LL << (bits - 1)) - 1;
                        if ((int64_t)num < min_val || (int64_t)num > max_val)
                            aparse__raise_fatal(ctx, APARSE_STATUS_OVERFLOW, arg, argv);
                    }
                } else {
                    uint64_t max_val = bits == 64 ? UINT_MAX : ((1ULL << bits) - 1);
                    if (num > max_val)
                        aparse__raise_fatal(ctx, APARSE_STATUS_OVERFLOW, arg, argv);
                }
            }

            memcpy(arg->ptr, 
                    (uint8_t*)&num + (APARSE_IS_LE ? 0 : 8 - arg->size), arg->size);

            if (have_sign) {
                uint8_t* msb = (uint8_t*)arg->ptr + 
                    (APARSE_IS_LE ? arg->size - 1 : 0);
                *msb = (uint8_t)((*msb & ~0x80) | (((int64_t)num < 0) ? 0x80 : 0));
            }
            break;
        }
        case APARSE_ARG_TYPE_FLOAT:
        {
            aparse_status status = aparse__process_float(argv, arg);
            if(status != APARSE_STATUS_OK)
            {
                if(status != APARSE_STATUS_UNDERFLOW)
                    aparse__raise_fatal(ctx, status, arg, argv)
                else
                    aparse__raise_nonfatal(ctx, status, arg, argv);
            }
            break;
        }
        default:
            aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_TYPE, arg, 0);
    }
    return APARSE_STATUS_OK;
}


static aparse_status aparse__process_parser(
        const int argc,
        const char* cargv,
        char* const* argv,
        aparse_arg* arg,
        aparse__context_t* ctx)
{
    aparse_status ret = APARSE_STATUS_OK;
    aparse_arg *subparser = 0;
    uint8_t* buffer = 0;
    int invalid_idx = 0;
    size_t min_size = 0;

    if(!arg->subargs)
    {
        aparse__raise_nonfatal(ctx, APARSE_STATUS_NULL_POINTER, 
                arg, NULL);
        return APARSE_STATUS_OK;
    }
    
    aparse__foreach(item, arg)
    {
        if(!strcmp(cargv, item->longopt))
        {
            subparser = item;
            break;
        }
    }
    if(!subparser) 
    {
        aparse_list arg_list = { .ptr = (void*)arg->subargs };
        for(aparse_arg* copy = arg_list.ptr; aparse_arg_nend(copy); copy++)
            arg_list.size++;
        aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_SUBCOMMAND, 
                &arg_list, cargv);
    }

    if(!subparser->subargs)
    {
        aparse_list_add(ctx->dispatch, 
                (aparse__dispatch_t[1]){{subparser, NULL}});
        return APARSE_STATUS_OK;
    }

    if(subparser->layout_size != 0)
    {   
        size_t last_idx = subparser->layout_size - 1;
        if(!aparse__verify_layout(subparser, &invalid_idx))
        {
            aparse__raise_fatal(ctx, 
                    APARSE_STATUS_INVALID_LAYOUT, subparser, &invalid_idx);
        }
        min_size =
                subparser->data_layout[last_idx * 2] + 
                subparser->data_layout[last_idx * 2 + 1];
        if(!subparser->ptr)
        {
            buffer = calloc(min_size, sizeof(*buffer));
            if(!buffer)
                aparse__raise_fatal(ctx, APARSE_STATUS_ALLOC_FAILURE, 0, 0);
        } else {
            if(subparser->size < min_size)
                aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_SIZE,
                        subparser, &subparser->size);
            buffer = subparser->ptr;
        }
    }

    aparse__fill_args_dest(subparser, buffer);

    ret = aparse__parse_impl(argc, argv, subparser->subargs, ctx);
    if(ret == APARSE_STATUS_OK)
        ret = aparse__check_missing(ctx, subparser->subargs);

    if(!subparser->handler || ret != APARSE_STATUS_OK)
        free(buffer);
    else {
        aparse_list_add(ctx->dispatch, 
                (aparse__dispatch_t[1])
                {{
                    .args = subparser, 
                    .payload = buffer
                }});
    }
    return ret;
}

static aparse_status aparse_process_optional(
        const int argc, 
        char* const* argv, 
        aparse_arg* arg,
        aparse__context_t *ctx)
{
    int *idx = &ctx->idx;
    if(aparse__type_cmp(arg, APARSE_ARG_TYPE_BOOL))
    {
        bool was_set = false;
        uint8_t *lsb = NULL;

        if(arg->flags & APARSE__ARG_PROCESSED)
            return APARSE_STATUS_FAILURE;
        for(size_t i = 0; i < arg->size; i++)
        {
            if(((char*)arg->ptr)[i])
                was_set = true;
        }

        // Attempting to set the LSB to 1
        memset(arg->ptr, 0, arg->size);
        lsb = (uint8_t*)arg->ptr + (APARSE_IS_LE ? 0 : arg->size - 1);
        if(!was_set) 
            *lsb |= 1;
        arg->flags |= APARSE__ARG_PROCESSED;
        return APARSE_STATUS_OK;
    }
    // if has equal
    if(arg->flags & APARSE__ARG_EQUAL_VAL) 
    {
        const char* optname = (arg->flags & APARSE__ARG_SHORT_MATCH) ? 
            arg->shortopt : arg->longopt;
        return aparse__process_argument(
                argv[*idx - 1] + 1 + strlen(optname), 
                arg,
                ctx);
    }
    else {
        if(*idx >= argc) {
            int expected_count = 1;
            aparse__raise_fatal(ctx, APARSE_STATUS_MISSING_VALUE, arg, &expected_count);
        }
        (*idx)++;
        return aparse__process_argument(
                argv[*idx - 1], 
                arg, 
                ctx);
    }
}

static aparse_status aparse__process_float(
        const char* argv, 
        const aparse_arg* arg)
{
    char* endptr = 0;
    errno = 0;
    long double num = strtold(argv, &endptr);
    long double abs_val = fabsl(num);

    if(*endptr)
        return APARSE_STATUS_INVALID_VALUE;
    
    if(errno == ERANGE)
    {
        if((num == HUGE_VALL || num == -HUGE_VALL))
            return APARSE_STATUS_OVERFLOW;
        if(num == 0.0 || abs_val < LDBL_MIN)
            return APARSE_STATUS_UNDERFLOW;
    }

    switch (arg->size) 
    {
        case sizeof(float):
            if (abs_val > FLT_MAX && abs_val != INFINITY)
                return APARSE_STATUS_OVERFLOW;
            if (num != 0.0L && abs_val < FLT_MIN)
                return APARSE_STATUS_UNDERFLOW;
            *(float *)arg->ptr = (float)num;
            break;

        case sizeof(double):
            if (abs_val > DBL_MAX && abs_val != INFINITY)
                return APARSE_STATUS_OVERFLOW;
            if (num != 0.0L && abs_val < DBL_MIN)
                return APARSE_STATUS_UNDERFLOW;
            *(double *)arg->ptr = (double)num;
            break;

        case sizeof(long double):
            *(long double *)arg->ptr = num;
            break;

        default:
            return APARSE_STATUS_UNHANDLED;
    }
    return 0;
}

static aparse_status aparse__process_array(
        const int argc, 
        char* const * argv, 
        aparse_arg* arg, 
        aparse__context_t *ctx)
{
    int *idx = &ctx->idx;
    aparse_list* dest = arg->ptr;
    size_t arrsz = 0, increment = 0;

    if(!dest)
        aparse__raise_fatal(ctx, APARSE_STATUS_NULL_POINTER, arg, 0);
    if(arg->size < sizeof(aparse_list))
        aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_SIZE, arg, &arg->size);
    if((arg->type & APARSE_ARG_TYPE_BITMASK) == APARSE_ARG_TYPE_UNKNOWN)
        aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_TYPE, arg, 0);

    (*idx)--;
    arrsz = (size_t)(argc - *idx);
    increment = arg->type & APARSE_ARG_TYPE_STRING ? 
        sizeof(char*) : 
        arg->element_size;

    if(increment <= 0)
        aparse__raise_fatal(ctx, APARSE_STATUS_INVALID_SIZE, arg, &arg->element_size);

    if(arrsz < arg->array_size)
        aparse__raise_fatal(ctx, APARSE_STATUS_MISSING_VALUE, arg, &arrsz);
    
    if(!aparse_list_new(dest,  arrsz, increment))
        aparse__raise_fatal(ctx, APARSE_STATUS_ALLOC_FAILURE, 0, 0);
    arg->ptr = dest->ptr;
    arg->size = arg->element_size;
    while(dest->size < dest->capacity)
    {
        if(aparse__process_argument(
                    argv[(*idx)], 
                    arg,
                    ctx) != APARSE_STATUS_OK)
            return APARSE_STATUS_FAILURE;
        dest->size++;
        (*idx)++; 
        arg->ptr = (uint8_t*)arg->ptr + increment;
    }
    // re-assign to prevent SIGSEGV in aparse_destroy_data
    arg->ptr = dest;
    return APARSE_STATUS_OK;
}

// Use to evaluate the size of given argument, therefore checking it with data layout
static size_t aparse__eval_size(
        const aparse_arg* arg)
{
    if(arg->type & APARSE_ARG_TYPE_ARRAY)
        return sizeof(aparse_list);
    if(arg->size == 0 && aparse__type_cmp(arg, APARSE_ARG_TYPE_STRING))
        return sizeof(char*);
    return arg->size;
}

static bool aparse__verify_layout(
        const aparse_arg *arg,
        int *invalid_idx)
{
    size_t prev_offset = 0, prev_size = 0;
    const size_t *layout = arg->data_layout;
    size_t layout_size = arg->layout_size;

    const aparse_arg *arg_ptr = arg->subargs;

    if (!arg->subargs && !layout)
        return false;

    if (!arg->subargs || !layout || layout_size <= 0)
    {
        *invalid_idx = -1;
        return false;
    }

    for(size_t i = 0; i < layout_size; i++)
    {
        size_t offset = 0, size = 0;
        if (!aparse_arg_nend(arg_ptr))
        {
            *invalid_idx = -1;
            return 0;
        }
        if (!aparse__is_argument(arg_ptr))
        {
            arg_ptr++;
            continue;
        }
        offset = layout[i * 2];
        size   = layout[i * 2 + 1];

        if (size == 0 ||
            offset < prev_offset + prev_size ||
            aparse__eval_size(arg_ptr) > size)
        {
            *invalid_idx = (int)i;
            return false;
        }

        prev_offset = offset;
        prev_size = size;
        arg_ptr++;
    }

    if(aparse_arg_nend(arg_ptr))
    {
        *invalid_idx = -1;
        return false;
    }

    return true;
}

static int aparse__fill_args_dest(
        const aparse_arg* arg, 
        uint8_t *buffer)
{
    size_t layout_size = 0;
    aparse_arg *arg_ptr = 0;
    if(!buffer)
        return APARSE_STATUS_OK;

    layout_size = arg->layout_size;
    if(layout_size == 0)
        return APARSE_STATUS_OK;

    arg_ptr = arg->subargs;
    for(size_t i = 0; i < layout_size && aparse_arg_nend(arg_ptr); 
            i++, arg_ptr++)
    {
        size_t offset = 0;
        if(!aparse__is_argument(arg_ptr))
            continue;
        offset  = arg->data_layout[i * 2];

        if(arg_ptr->ptr)
            continue;
        
        arg_ptr->ptr = &buffer[offset];
        if(arg_ptr->size == 0 && 
                aparse__type_cmp(arg_ptr, APARSE_ARG_TYPE_STRING))
            continue;
        arg_ptr->size = aparse__eval_size(arg_ptr);
    }
    return APARSE_STATUS_OK;
}

static void aparse__destroy_payload(
        const aparse_arg *args, 
        uint8_t *payload)
{
    if(!args || !payload)
        return;
    aparse__foreach(sa, args)
    {
        if(!(sa->type & APARSE_ARG_TYPE_ARRAY))
            continue;
        free(((aparse_list*)sa->ptr)->ptr);
    }
}

// 0 no error, 1 error (just for cleaning up)
static aparse_status aparse__check_missing(
        aparse_context* ctx,
        aparse_arg* args) 
{
    aparse_list missing_args = {.itemsz = sizeof(aparse_arg*)};
    aparse__tillend(item, args)
    {
        if(
                aparse__is_positional(item) &&
                !(item->flags & APARSE__ARG_PROCESSED) &&
                !(item->type & APARSE_ARG_TYPE_ARRAY && item->array_size == 0))
            aparse_list_add(&missing_args, &item);
    }
    if(missing_args.size > 0)
    {
        aparse__raise_fatal(ctx, APARSE_STATUS_MISSING_POSITIONAL, 
                &missing_args, NULL);
        aparse_list_free(&missing_args);
    }

    return APARSE_STATUS_OK;
}

static void aparse__default_errcb(
        const aparse_context* ctx,
        const aparse_status status, 
        const void* field1, 
        const void* field2, 
        void* userdata)
{
    switch(status)
    {
        case APARSE_STATUS_UNKNOWN_ARGUMENT:
        {
            const aparse_list* args = field1;
            fprintf(stderr, "%s: " APARSE__ERROR_LABEL 
                    ": unrecognized arguments: ", aparse_progname);
            char** unknowns = args->ptr;
            for (size_t i = 0; i < args->size; i++, unknowns++) {
                if (i > 0) fprintf(stderr, ", ");
                fprintf(stderr, "%s", *unknowns);
            }
            fprintf(stderr, "\n");
            break;
        }
        case APARSE_STATUS_MISSING_VALUE:
        {
            const aparse_arg* arg = field1;
            const int expected_count = *(const int*)field2;
            aparse_prog_error("option '%s' expected %d argument.",
                arg->flags & APARSE__ARG_SHORT_MATCH ? arg->shortopt : arg->longopt,
                expected_count);
            break;
        }
        case APARSE_STATUS_INVALID_VALUE:
        {
            const aparse_arg* arg = field1;
            const char* cargv = field2;
            aparse_prog_error("invalid %s '%s'",
                arg->type & APARSE_ARG_TYPE_FLOAT ? "float" : "integer", cargv);
            break;
        }
        case APARSE_STATUS_OVERFLOW:
        {
            const aparse_arg* arg = field1;
            const char* cargv = field2;
            aparse__lib__error("value '%s' is too large for argument '%s'",
                                     cargv, arg->longopt ? arg->longopt : arg->shortopt);
            break;
        }
        case APARSE_STATUS_UNDERFLOW:
        {
            const aparse_arg* arg = field1;
            const char* cargv = field2;
            if(arg->type & APARSE_ARG_TYPE_FLOAT)
            {
                aparse__lib__warn(
                        "value '%s' underflows precision argument '%s'",
                        cargv, arg->longopt ? arg->longopt : arg->shortopt);
            } else {
                aparse__lib__error(
                        "value '%s' underflows argument '%s'",
                        cargv, arg->longopt ? arg->longopt : arg->shortopt);
            }
            break;
        }
        case APARSE_STATUS_MISSING_POSITIONAL:
        {
            const aparse_list* args = field1;
            aparse__print_usage(ctx);
            fprintf(stderr, "%s: " APARSE__ERROR_LABEL 
                    ": the following arguments are required: ", aparse_progname);
            int printed = 0;
            for (size_t i = 0; i < args->size; i++) 
            {
                aparse_arg *item = aparse_list_get(args, aparse_arg*, i);
                if (printed++ > 0) 
                    fprintf(stderr, ", ");
                fprintf(stderr, "%s", item->longopt);
            }
            fprintf(stderr, "\n");
            break;
        }
        case APARSE_STATUS_INVALID_SUBCOMMAND:
        {
            const aparse_list* args = field1;
            const char* cargv = field2;
            aparse__print_usage(ctx);
            fprintf(stderr, "%s: " APARSE__ERROR_LABEL 
                    ": invalid choice: '%s' (choose from ", aparse_progname, cargv);
            aparse_arg* a = args->ptr;
            for(size_t i = 0; i < args->size; i++, a++)
            {
                fprintf(stderr, "%s%s", a->longopt, 
                        i < (args->size - 1) ? ", " : "");
            }

            fprintf(stderr, ")\n");
            break;
        }
        case APARSE_STATUS_NULL_POINTER:
        {
            const aparse_arg* arg = field1;
            aparse__lib__warn(
                    "non-null pointers was expected of argument '%s', skipped.",
                    arg->longopt ? arg->longopt : arg->shortopt);
            break;
        }
        case APARSE_STATUS_INVALID_TYPE:
        {
            const aparse_arg* arg = field1;
            aparse__lib__error(
                    "invalid argument type: 0x%04X of argument '%s'",
                    arg->type, arg->longopt ? arg->longopt : arg->shortopt);
            break;
        }
        case APARSE_STATUS_INVALID_SIZE:
        {
            const aparse_arg* arg = field1;
            const int size = *(const int*)field2;
            aparse__lib__error(
                    "invalid argument size: %d bytes of argument '%s'",
                    size, arg->longopt ? arg->longopt : arg->shortopt);
            break;
        }
        case APARSE_STATUS_INVALID_LAYOUT:
        {
            const aparse_arg* arg = field1;
            const int index = *(const int*)field2;
            aparse__lib__error("layout of \"%s\" at index %d was invalid",
                    arg->longopt, index);
            break;
        }
        case APARSE_STATUS_ALLOC_FAILURE:
        {
            aparse__lib__error(
                    "failed to allocate memory for argument parsing, "
                    "please retry again later.");
            break;
        }
        case APARSE_STATUS_UNHANDLED:
        {
            const aparse_arg* arg = field1;
            aparse__lib__error(
                    "unhandled %s size: %zu bytes of argument: '%s'",
                    arg->type & APARSE_ARG_TYPE_FLOAT ? "float" : "integer",
                    arg->size, arg->longopt ? arg->longopt : arg->shortopt
            );
            break;
        }

        case APARSE_STATUS_TOO_DEEP:
        {
            aparse__lib__error("parser nesting depth exceeded the limit");
            break;
        }
        default:
        {
            aparse__lib__error("unhandled error message");
            break;
        }
    }
}

static void aparse__print_help(
        aparse_arg* main_args,
        aparse__context_t *ctx) 
{
    aparse__print_usage(ctx);
    printf("\n");
    if(ctx->stack_top == 1) 
    {
        if(aparse__desc)
            printf("%s\n\n", aparse__desc);
    }
    printf("positional arguments:\n");
    aparse__print_pos_help(main_args);
    printf("\noptions:\n");
    aparse__print_help_tag(&aparse__help_arg, APARSE__SPACE_PER_INDENT);

    for(aparse_arg *sa = main_args;
            aparse_arg_nend(sa); sa++)
    {
        if (
                aparse__is_argument(sa) && 
                !aparse__is_positional(sa))
            aparse__print_help_tag(sa, APARSE__SPACE_PER_INDENT);
    }
}

static void aparse__print_wrapped(
        const char *text,
        const int start_col,
        const int term_width)
{
    int usable_width = term_width - start_col;
    int col = 0;
    const char *p = text;

    printf("%*s", start_col, "");
    while (*p)
    {
        const char *word_start = NULL;
        int word_len = 0;

        if (*p == '\n')
        {
            putchar('\n');
            printf("%*s", start_col, "");
            col = 0;
            p++;
            continue;
        }

        while (*p && isspace((unsigned char)*p) && *p != '\n')
            p++;

        word_start = p;

        while (*p && !isspace((unsigned char)*p))
        {
            p++;
            word_len++;
        }

        if (word_len == 0)
            continue;

        int extra = (col > 0) ? 1 : 0;

        if (col && (col + extra + word_len > usable_width))
        {
            putchar('\n');
            printf("%*s", start_col, "");
            col = 0;
            extra = 0;
        }

        if (extra)
        {
            putchar(' ');
            col += 1;
        }

        fwrite(word_start, 1, (size_t)word_len, stdout);
        col += word_len;
    }
}
static void aparse__print_help_tag(
        const aparse_arg* arg,
        const int indent)
{
    int len = 0;
    if (!aparse__is_argument(arg) && !arg->help && !aparse_arg_nend(arg))
        return;


    len += printf("%*s", indent, "");
    if (arg->shortopt)
        len += printf("%s", arg->shortopt);
    if (arg->shortopt && arg->longopt)
        len += printf(", ");
    if (arg->longopt)
        len += printf("%s", arg->longopt);

    if (!aparse__is_positional(arg) &&
        arg->type != APARSE_ARG_TYPE_BOOL)
    {
        const char* option = arg->longopt ? arg->longopt : arg->shortopt;
        size_t idx = aparse__option_value_index(option);

        putchar(' ');
        len += 1;

        for (const char* ch = option + idx; *ch != '\0'; ch++)
        {
            putchar(toupper((unsigned char)*ch));
            len += 1;
        }
    }

    bool longer = len > MAX_ARG_STR;
    if (arg->help)
    {
        int space = APARSE__SPACE_PER_INDENT + 
            (longer ? APARSE__SPACE_PER_INDENT + MAX_ARG_STR : MAX_ARG_STR - len);

        if (longer)
            printf("\n");

        aparse__print_wrapped(
                arg->help, 
                space, 
                aparse__get_term_width());
    }

    printf("\n");
}

static void aparse__print_pos_help(aparse_arg* args) 
{
    for (; aparse_arg_nend(args); args++) {
        if (aparse__is_positional(args)) {
            if(aparse__is_argument(args)) {
                aparse__print_help_tag(args, APARSE__SPACE_PER_INDENT);
                continue;
            }
            printf("%*s", APARSE__SPACE_PER_INDENT, "");
            aparse__print_subcmds(args);
            printf("\n");

            aparse__foreach(ptr, args)
            {
                aparse__print_help_tag(ptr, 
                        APARSE__SPACE_PER_INDENT * 2);
            }
        }
    }
}

static void aparse__print_subcmds(
        const aparse_arg* args) 
{
    int index = 0;
    if (!args || !args->subargs)
    {
        fputs("{}", stderr);
        return;
    }

    fputc('{', stderr);
    aparse__foreach(ptr, args)
    {
        const char* name = ptr->longopt ? ptr->longopt : "";
        if (index++ > 0)
            fputs(", ", stderr);
        fputs(name, stderr);
    }

    fputc('}', stderr);
}

static void aparse__print_usage_before(
        const aparse_context *ctx) 
{
    int idx = 0;
    aparse_arg *arg = NULL;

    for(;;)
    {
        bool found = false;
        if(idx >= ctx->stack_top - 1)
            break;

        arg = ctx->stack[idx];
        for(; aparse_arg_nend(arg); arg++)
        {
            if(!aparse__is_positional(arg))
                continue;

            if(aparse__is_argument(arg))
                fprintf(stderr, "%s ", arg->longopt);
            else
            {
                aparse__foreach(subcmd, arg)
                {
                    if(subcmd->subargs == ctx->stack[idx + 1])
                    {
                        idx++;
                        found = true;
                        fprintf(stderr, "%s ", subcmd->longopt);
                        break;
                    }
                }

                if(found)
                    break;
            }
        }

        // unlikely, this meant that stack[idx] and 
        // stack[idx + 1] has no relation
        if(!found)
            break;
    }
}

static void aparse_print_usage_after(aparse_arg* args)
{
    aparse_list list = {.itemsz = sizeof(aparse_arg*)};
    for(aparse_arg *sa = args;
            aparse_arg_nend(sa); sa++)
    {
        if(aparse__is_positional(sa))
            aparse_list_add(&list, &sa);
        else 
        {
            fprintf(stderr, "[%s", 
                    sa->shortopt ? sa->shortopt : sa->longopt);
            if(!aparse__type_cmp(sa, APARSE_ARG_TYPE_BOOL))
            {
                const char *option = 0;
                size_t idx = 0;
                
                option = sa->longopt ? sa->longopt : sa->shortopt;
                idx = aparse__option_value_index(option);
                fputc(' ', stderr);
                for(const char *ch = option + idx; *ch != '\0'; ch++)
                    fputc(toupper(*ch), stderr);
            }
            fputs("] ", stderr);
        }
    }
    for(size_t i = 0; i < list.size; i++)
    {
        aparse_arg* entry = aparse_list_get(&list, aparse_arg*, i);
        if(aparse__is_argument(entry))
            fprintf(stderr, "%s ", entry->longopt);
        else {
            aparse__print_subcmds(entry);
            fputs(" ... ", stderr);
        }
    }
    aparse_list_free(&list);
    fputc('\n', stderr);
}

static void aparse__print_usage(
        const aparse_context *ctx) 
{
    fprintf(stderr, "usage: %s ", aparse_progname);
    aparse__print_usage_before(ctx);
    aparse_print_usage_after(ctx->stack[ctx->stack_top - 1]);
}

static void aparse__reset_state(
        aparse_arg *args)
{
    if(!args)
        return;
    aparse__tillend(arg, args)
    {
        if(
                aparse__is_positional(arg) && 
                !aparse__is_argument(arg))
        {
            aparse__foreach(subcmd, arg)
                aparse__reset_state(subcmd->subargs);
        }

        arg->flags = 0;
    }
}

static aparse_arg* aparse__argv_match(
        const char* argv, 
        aparse_arg* args)
{
    aparse_arg* positional = NULL;
    if(
            !strcmp(argv, aparse__help_arg.shortopt) || 
            !strcmp(argv, aparse__help_arg.longopt))
        return (aparse_arg*)(uintptr_t)&aparse__help_arg;
    
    for(aparse_arg *sa = args;
            aparse_arg_nend(sa); sa++)
    {
        if (aparse__is_positional(sa)) {
            if (!(sa->flags & APARSE__ARG_PROCESSED))
                if(!positional)
                    positional = sa;
            continue;
        }
        if (sa->shortopt) 
        {
            size_t shortlen = strlen(sa->shortopt);
            if (strncmp(argv, sa->shortopt, shortlen) == 0) {
                sa->flags |= APARSE__ARG_SHORT_MATCH;
                if (argv[shortlen] == '\0') {
                    return sa;
                } else if (argv[shortlen] == '=') {
                    sa->flags |= APARSE__ARG_EQUAL_VAL;
                    return sa;
                }
            }
        }
        if (sa->longopt) 
        {
            size_t longlen = strlen(sa->longopt);
            if (strncmp(argv, sa->longopt, longlen) == 0) {
                sa->flags &= (uint8_t)~APARSE__ARG_SHORT_MATCH;
                if (argv[longlen] == '\0') {
                    return sa;
                } else if (argv[longlen] == '=') {
                    sa->flags |= APARSE__ARG_EQUAL_VAL;
                    return sa;
                }
            }
        }
    }
    return positional;
}

static const char* aparse__get_exename(
        const char* argv0)
{
    const char* last_slash = strrchr(argv0, '/');
#ifdef _WIN32
    const char* last_backslash = strrchr(argv0, '\\');
    if (!last_slash || (last_backslash && last_backslash > last_slash))
        last_slash = last_backslash;
#endif
    return last_slash ? last_slash + 1 : argv0;
}

static size_t aparse__option_value_index(
        const char* longopt)
{
    const char* p = 0;
    size_t idx = 0;

    p = longopt;
    if(!p)
        return 0;

    while(p[idx] == '-' || p[idx] == '/')
        idx++;

    return idx;
}

static int aparse__get_term_width(void)
{
    int width = 0;
    char *env = NULL;

    env = getenv("COLUMNS");
    if (env)
    {
        width = atoi(env);
        if (width > 0)
            return width;
    }

#ifdef _WIN32
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
        HANDLE h = 0;

        h = GetStdHandle(STD_OUTPUT_HANDLE);

        if (h != INVALID_HANDLE_VALUE &&
            GetConsoleScreenBufferInfo(h, &csbi))
        {
            width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            if (width > 0)
                return width;
        }
    }

#else
    {
        struct winsize ws = {0};
        if (isatty(STDOUT_FILENO))
        {
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
            {
                if (ws.ws_col > 0)
                    return ws.ws_col;
            }
        }
    }
#endif

    return 80;
}
/* ------------/home/binaryfox0/proj/aparse/src/aparse.c END------------ */

/* --------/home/binaryfox0/proj/aparse/src/aparse_list.c BEGIN--------- */

#define min(a, b) ((a) < (b) ? (a) : (b))

int aparse_list_new(
        aparse_list* list, 
        const size_t init_size, 
        const size_t var_size) 
{
    if(!list || var_size == 0)
        return 0;
    list->itemsz = var_size;
    list->size = 0;
    list->ptr = NULL;
    list->capacity = 0;
    return init_size > 0 ? aparse_list_resize(list, init_size) : 1;
}

int aparse_list_resize(
        aparse_list* list, 
        const size_t new_size) 
{
    if (!list || !list->itemsz)
        return 0;

    if (new_size) 
    {
        void* tmp = realloc(list->ptr, new_size * list->itemsz);
        if (!tmp) 
            return 0;
        list->ptr = tmp;
        list->capacity = new_size;
        list->size = min(list->size, new_size);
    } else {
        free(list->ptr);
        list->ptr = NULL;
        list->capacity = 0;
        list->size = 0;
    }
    return 1;
}

int aparse_list_add(
        aparse_list* list, 
        const void* data) 
{
    if (!list->itemsz)
        return 0;

    if (list->size >= list->capacity) 
    {
        size_t new_capacity = list->capacity ? list->capacity * 2 : 4;
        if (!aparse_list_resize(list, new_capacity))
            return 0;
    }

    memcpy((uint8_t*)list->ptr + list->size * list->itemsz, data, list->itemsz);
    list->size++;
    return 1;
}

void aparse_list_free(aparse_list* list) 
{
    if(!list)
        return;
    free(list->ptr);
    memset(list, 0, sizeof(*list));
}
/* ---------/home/binaryfox0/proj/aparse/src/aparse_list.c END---------- */


#endif /* APARSE_IMPLEMENTATION */
#endif /* APARSE_H */
