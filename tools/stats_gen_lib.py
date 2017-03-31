#!/usr/bin/python

from string import Formatter

_STATS_TEMPLATE = '''#ifndef {INCLUDE_GUARD}
#define {INCLUDE_GUARD}

// clang-format off
#ifdef {MACRO_PREFIX}ON
{INCLUDES}

    #include <iomanip>
    #include <sstream>
    #include <string>
    #include <vector>

    // comment out any stats you don't want, and things will be just dandy
{MACROS_ON}
    #define {MACRO_PREFIX}PUBLISH_RECORD()                                                       \\
        do {{                                                                                       \\
            {NS_ACCESS}{CLASS_NAME}::get().publish({NS_ACCESS}tls_record());                   \\
            {NS_ACCESS}tls_record() = {{}};                                                       \\
        }} while(0)                                                                                 \\
    /**/
    #define {MACRO_PREFIX}CLEAR() {NS_ACCESS}{CLASS_NAME}::get().clear()
    #ifndef {MACRO_PREFIX}DUMP
        #include <iostream>
        #define {MACRO_PREFIX}DUMP() (std::cout << {NS_ACCESS}{CLASS_NAME}::get().results())
    #endif /* {MACRO_PREFIX}DUMP */
#else
    #define {MACRO_PREFIX}PUBLISH_RECORD()                               /**/
    #define {MACRO_PREFIX}CLEAR()                                        /**/
    #ifndef {MACRO_PREFIX}DUMP
        #define {MACRO_PREFIX}DUMP()                                     /**/
    #endif /* {MACRO_PREFIX}DUMP */
#endif /* {MACRO_PREFIX}ON */

{MACROS_OFF}

// clang-format on

#ifdef {MACRO_PREFIX}ON

{NAMESPACE_BEGIN}
    struct {CLASS_NAME}_tls_record
    {{
{THREAD_RECORD_MEMBERS}

        {CLASS_NAME}_tls_record() noexcept = default;
        
{THREAD_RECORD_MEMBER_FUNCTIONS}

        std::string results() const
        {{
            std::ostringstream ostr;
            ostr
{THREAD_RECORD_STREAM_OUTPUT};
            return ostr.str();
        }}
    }};

    inline {CLASS_NAME}_tls_record& tls_record() noexcept
    {{
        static LSTM_THREAD_LOCAL {CLASS_NAME}_tls_record record{{}};
        return record;
    }}

    struct {CLASS_NAME}
    {{
    private:
        using records_t          = std::vector<{CLASS_NAME}_tls_record>;
        using records_iter       = typename records_t::iterator;
        using records_value_type = typename records_t::value_type;

        records_t records_;

        {CLASS_NAME}()                       = default;
        {CLASS_NAME}(const {CLASS_NAME}&) = delete;
        {CLASS_NAME}& operator=(const {CLASS_NAME}&) = delete;

        std::uint64_t
        total_count(std::function<std::size_t(const {CLASS_NAME}_tls_record*)> accessor) const noexcept
        {{
            std::size_t result = 0;
            for (auto& tid_record : records_)
                result += accessor(&tid_record);
            return result;
        }}

        std::uint64_t
        max(std::function<std::size_t(const {CLASS_NAME}_tls_record*)> accessor) const noexcept
        {{
            std::size_t result = 0;
            for (auto& tid_record : records_)
                result = std::max(result, accessor(&tid_record));
            return result;
        }}

    public:
        static {CLASS_NAME}& get() noexcept
        {{
            static {CLASS_NAME} singleton;
            return singleton;
        }}

        inline void publish({CLASS_NAME}_tls_record record) noexcept
        {{
            records_.emplace_back(std::move(record));
        }}

{TRANSACTION_LOG_MEMBER_FUNCTIONS}

        std::size_t thread_count() const noexcept {{ return records_.size(); }}

        const records_t& records() const noexcept {{ return records_; }}

        void clear() noexcept {{ records_.clear(); }}

        std::string results(bool per_thread = true) const
        {{
            std::ostringstream ostr;
            ostr
{TRANSACTION_LOG_STREAM_OUTPUT};

            if (per_thread) {{
                std::size_t i = 0;
                for (auto& record : records_) {{
                    ostr << "--== Thread: " << std::setw(4) << i++ << " ==--" << '\\n';
                    ostr << record.results() << '\\n';
                }}
            }}
            return ostr.str();
        }}
    }};
{NAMESPACE_END}

#endif /* {MACRO_PREFIX}ON */

#endif /* {INCLUDE_GUARD} */'''

types = {
    'counter' : 'std::uint64_t',
    'max'     : 'std::uint64_t',
    'sum'     : 'std::uint64_t',
}

def indent(s, amount = 1):
    return '\n'.join([' ' * amount * 4 + x for x in s.splitlines()])

def get_pretty_name(stat):
    return ' '.join([w.capitalize() for w in stat.split()])

def get_mem_name(stat):
    return stat.lower().replace(' ', '_')
    
def get_mem_fun_for_stat(compound_stat):
    return get_mem_name(compound_stat) + '()'
    
def get_mem_or_func_call(stat, stats, compound_stats):
    if stat in stats:
        return get_mem_name(stat)
    assert(stat in compound_stats)
    return get_mem_fun_for_stat(stat)

def get_macro_name(stat, macro_prefix):
    return macro_prefix + stat.upper().replace(' ', '_')
    
def get_macro_params(stat, stats_kinds):
    param = {
        'counter' : '',
        'max'     : 'amt',
        'sum'     : 'amt',
    }
    return param[stats_kinds[stat]]
    
def get_macro_define(stat, stats_kinds, macro_prefix):
    return '#define %s(%s)' % (
        get_macro_name(stat, macro_prefix),
        get_macro_params(stat, stats_kinds)
    )

def add_trailing_whitespace(strings):
    max_length = max([len(x) for x in strings])
    return ['{0: <{1}}'.format(x, max_length + 1) for x in strings]
    
    
def get_macro_defines(stats, stats_kinds, macro_prefix):
    return add_trailing_whitespace([get_macro_define(stat, stats_kinds, macro_prefix)
                                    for stat in stats])
    
def get_macro_expansion_on(stat, stats_kinds, ns_access):
    param = {
        'counter' : '++{NS_ACCESS}tls_record().{MEM_NAME}',
        'max'     : '{NS_ACCESS}tls_record().{MEM_NAME} = std::max({NS_ACCESS}tls_record().{MEM_NAME}, static_cast<std::uint64_t>({PARAMS}))',
        'sum'     : '{NS_ACCESS}tls_record().{MEM_NAME} += {PARAMS}',
    }
    return param[stats_kinds[stat]].format(
        NS_ACCESS = ns_access,
        MEM_NAME = get_mem_name(stat),
        PARAMS = get_macro_params(stat, stats_kinds)
    )

def get_macros_on(stats, stats_kinds, ns_access, macro_prefix):
    param = {
        'counter' : '++{MEM_NAME}',
        'max'     : '{MEM_NAME} = std::max({MEM_NAME}, {PARAMS})',
        'sum'     : '{MEM_NAME} += {PARAMS}',
    }
    defines = get_macro_defines(stats, stats_kinds, macro_prefix)
    return '\n'.join([define + get_macro_expansion_on(stat, stats_kinds, ns_access)
                      for stat, define in zip(stats, defines)])

def get_macros_off(stats, stats_kinds, macro_prefix):
    _FORMAT_STRING = '''#ifndef {MACRO_NAME}
    {MACRO_DEFINE} /**/
#endif'''

    result = []
    for stat in stats:
        result.append(_FORMAT_STRING.format(
            MACRO_NAME = get_macro_name(stat, macro_prefix),
            MACRO_DEFINE = get_macro_define(stat, stats_kinds, macro_prefix),
        ))
    return '\n'.join(result)

def get_thread_record_mems(stats, stats_kinds):
    initial_value = {
        'counter' : '0',
        'max'     : '0',
        'sum'     : '0',
    }
    _FORMAT_STRING = '%s %s{%s};'
    return '\n'.join([_FORMAT_STRING % (types[stats_kinds[stat]],
                                        get_mem_name(stat),
                                        initial_value[stats_kinds[stat]]) for stat in stats])

def map_get_mem_or_func_call(stat_list, stats, compound_stats):
    return [get_mem_or_func_call(x, stats, compound_stats) for x in stat_list]

def get_assert(op, operands):
    assert_kind = {
        '/' : ' <= ',
        '-' : ' >= ',
        '+' : None,
    }
    mems = map_get_mem_or_func_call(operands, stats, compound_stats)
    if assert_kind[op] != None:
        return 'LSTM_ASSERT(%s);\n    ' % assert_kind[op].join(mems)
    return ''

def get_contents(stats, compound_stats, stat_data):
    op = stat_data['op']
    operands = stat_data['operands']
    casted = map_get_mem_or_func_call(operands, stats, compound_stats)
    if op == '/':
        casted[-1] = 'float(%s)' % casted[-1]
    return (' ' + op + ' ').join(casted)

def get_thread_record_mem_fun(compound_stat, stats, compound_stats, compound_stats_kinds):
    _FORMAT_STRING = '''auto {NAME} const noexcept {{ return {CONTENTS}; }}'''
    return _FORMAT_STRING.format(
        NAME     = get_mem_fun_for_stat(compound_stat),
        CONTENTS = get_contents(stats, compound_stats, compound_stats_kinds[compound_stat]),
    )

def get_thread_record_mem_funs(stats, compound_stats, compound_stats_kinds):
    return '\n'.join([get_thread_record_mem_fun(compound_stat,
                                                stats,
                                                compound_stats,
                                                compound_stats_kinds)
                      for compound_stat in compound_stats])

def get_thread_record_stream_output(all_stats, stats, compound_stats):
    names = add_trailing_whitespace([get_pretty_name(s) + ':' for s in all_stats])
    values = add_trailing_whitespace([get_mem_or_func_call(s, stats, compound_stats) for s in all_stats])
    return '\n'.join(['<< "    ' + name + '" << ' + value + ' << \'\\n\''
                        for name, value in zip(names, values)])
                        
def get_singleton_class_mem_fun_contents(class_name,
                                         stat,
                                         stats,
                                         stats_kinds,
                                         compound_stats_kinds):
    if stat in stats:
        if stats_kinds[stat] == 'counter' or stats_kinds[stat] == 'sum':
            return 'total_count(&%s_tls_record::%s)' % (class_name, get_mem_name(stat))
        elif stats_kinds[stat] == 'max':
            return 'this->max(&%s_tls_record::%s)' % (class_name, get_mem_name(stat))
        else:
            assert(false)
    
    stat_data = compound_stats_kinds[stat]
    op = stat_data['op']
    operands = map(get_mem_fun_for_stat, stat_data['operands'])
    if op == '/':
        operands[-1] = 'float(%s)' % operands[-1]
    return (' ' + op + ' ').join(operands)

def get_singleton_class_mem_fun(class_name, stat, stats, stats_kinds, compound_stats_kinds):
    _FORMAT_STRING = '''auto {NAME} const noexcept {{ return {CONTENTS}; }}'''
    return _FORMAT_STRING.format(
        NAME     = get_mem_fun_for_stat(stat),
        CONTENTS = get_singleton_class_mem_fun_contents(class_name,
                                                        stat,
                                                        stats,
                                                        stats_kinds,
                                                        compound_stats_kinds),
    )

def get_singleton_class_mem_funs(class_name,
                                 stats,
                                 compound_stats,
                                 stats_kinds,
                                 compound_stats_kinds):
    return '\n'.join([get_singleton_class_mem_fun(class_name,
                                                  stat,
                                                  stats,
                                                  stats_kinds,
                                                  compound_stats_kinds)
                      for stat in stats] +
                     [get_singleton_class_mem_fun(class_name,
                                                  compound_stat,
                                                  stats,
                                                  stats_kinds,
                                                  compound_stats_kinds)
                      for compound_stat in compound_stats])

def get_singleton_class_stream_output(all_stats):
    names = add_trailing_whitespace([get_pretty_name(s) + ':' for s in all_stats])
    values = add_trailing_whitespace([get_mem_fun_for_stat(s)
                                      for s in all_stats])
    return '\n'.join(['<< "' + name + '" << ' + value + ' << \'\\n\''
                        for name, value in zip(names, values)])

def get_includes():
    return indent('''#include <lstm/detail/compiler.hpp>
#include <lstm/detail/namespace_macros.hpp>''')

all_stats_kinds = {
    'user failures'         : 'counter',
    'failures'              : 'counter',
    'successes'             : 'counter',
    'bloom collisions'      : 'counter',
    'bloom successes'       : 'counter',
    'quiesces'              : 'counter',
    'max write size'        : 'max',
    'max read size'         : 'max',
    'reads'                 : 'sum',
    'writes'                : 'sum',
    'transactions'          : {'op' : '+', 'operands': ['failures', 'successes']},
    'internal failures'     : {'op' : '-', 'operands' : ['failures', 'user failures']},
    'success rate'          : {'op' : '/', 'operands' : ['successes', 'transactions']},
    'failure rate'          : {'op' : '/', 'operands' : ['failures', 'transactions']},
    'internal failure rate' : {'op' : '/', 'operands' : ['internal failures', 'transactions']},
    'user failure rate'     : {'op' : '/', 'operands' : ['user failures', 'transactions']},
    'bloom checks'          : {'op' : '+', 'operands' : ['bloom successes', 'bloom collisions']},
    'bloom collision rate'  : {'op' : '/', 'operands' : ['bloom collisions', 'bloom checks']},
    'quiesce rate'          : {'op' : '/', 'operands' : ['quiesces', 'transactions']},
    'average read size'     : {'op' : '/', 'operands' : ['reads', 'transactions']},
    'average write size'    : {'op' : '/', 'operands' : ['writes', 'transactions']},
}

def gen_stats(
    the_stats,
    include_guard,
    class_name = 'perf_stats',
    macro_prefix = '',
    namespace_begin = '',
    namespace_end = '',
    namespace_access = '',
    stat_output_ordering = [],
    stats_member_ordering = [],
    compound_stats_member_func_ordering = []
    ):
    stats = stats_member_ordering
    stats += [k for k,v in the_stats.items() if type(v) == type('') and not k in stats]
    compound_stats = compound_stats_member_func_ordering
    compound_stats += [k for k,v in the_stats.items()
                          if type(v) != type('') and not k in compound_stats]
    
    stats_kinds = {k:v for k,v in the_stats.items() if type(v) == type('')}
    compound_stats_kinds = {k:v for k,v in the_stats.items() if type(v) != type('')}
    
    all_stats = stat_output_ordering
    all_stats += [k for k in the_stats.keys() if not k in all_stats]
    
    assert(sorted(all_stats) == sorted(compound_stats + stats))

    return _STATS_TEMPLATE.format(
        INCLUDE_GUARD = include_guard,
        MACRO_PREFIX = macro_prefix,
        INCLUDES = get_includes(),
        CLASS_NAME = class_name,
        NAMESPACE_BEGIN = namespace_begin,
        NAMESPACE_END = namespace_end,
        NS_ACCESS = namespace_access,
        MACROS_ON = indent(get_macros_on(stats, stats_kinds, namespace_access, macro_prefix), 1),
        MACROS_OFF = get_macros_off(stats, stats_kinds, macro_prefix),
        THREAD_RECORD_MEMBERS = indent(get_thread_record_mems(stats, stats_kinds), 2),
        THREAD_RECORD_MEMBER_FUNCTIONS = indent(get_thread_record_mem_funs(stats,
                                                                           compound_stats,
                                                                           compound_stats_kinds),
                                                2),
        THREAD_RECORD_STREAM_OUTPUT = indent(get_thread_record_stream_output(all_stats,
                                                                             stats,
                                                                             compound_stats), 4),
        TRANSACTION_LOG_MEMBER_FUNCTIONS = indent(get_singleton_class_mem_funs(class_name,
                                                                               stats,
                                                                               compound_stats,
                                                                               stats_kinds,
                                                                               compound_stats_kinds), 2),
        TRANSACTION_LOG_STREAM_OUTPUT = indent(get_singleton_class_stream_output(all_stats), 4),
    )