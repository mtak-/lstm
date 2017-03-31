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
    
{MACROS_ON}
    #define {MACRO_PREFIX}PUBLISH_RECORD()                                                              \\
        do {{                                                                                       \\
            {NS_ACCESS}transaction_log::get().publish({NS_ACCESS}tls_record());              \\
            {NS_ACCESS}tls_record() = {{}};                                                       \\
        }} while(0)
    /**/
    #define {MACRO_PREFIX}CLEAR() {NS_ACCESS}transaction_log::get().clear()
    #ifndef {MACRO_PREFIX}DUMP
        #include <iostream>
        #define {MACRO_PREFIX}DUMP() (std::cout << {NS_ACCESS}transaction_log::get().results())
    #endif /* {MACRO_PREFIX}DUMP */
#else
{MACROS_OFF}
    #define {MACRO_PREFIX}PUBLISH_RECORD()                               /**/
    #define {MACRO_PREFIX}CLEAR()                                        /**/
    #ifndef {MACRO_PREFIX}DUMP
        #define {MACRO_PREFIX}DUMP()                                     /**/
    #endif /* {MACRO_PREFIX}DUMP */
#endif /* {MACRO_PREFIX}ON */
// clang-format on

#ifdef {MACRO_PREFIX}ON

{NAMESPACE_BEGIN}
    struct thread_record
    {{
{THREAD_RECORD_MEMBERS}

        thread_record() noexcept = default;
        
{THREAD_RECORD_MEMBER_FUNCTIONS}

        std::string results() const
        {{
            std::ostringstream ostr;
            ostr
{THREAD_RECORD_STREAM_OUTPUT};
            return ostr.str();
        }}
    }};

    inline thread_record& tls_record() noexcept
    {{
        static LSTM_THREAD_LOCAL thread_record record{{}};
        return record;
    }}

    struct transaction_log
    {{
    private:
        using records_t          = std::vector<thread_record>;
        using records_iter       = typename records_t::iterator;
        using records_value_type = typename records_t::value_type;

        records_t records_;

        transaction_log()                       = default;
        transaction_log(const transaction_log&) = delete;
        transaction_log& operator=(const transaction_log&) = delete;

        std::uint64_t
        total_count(std::function<std::size_t(const thread_record*)> accessor) const noexcept
        {{
            std::size_t result = 0;
            for (auto& tid_record : records_)
                result += accessor(&tid_record);
            return result;
        }}

        std::uint64_t
        max(std::function<std::size_t(const thread_record*)> accessor) const noexcept
        {{
            std::size_t result = 0;
            for (auto& tid_record : records_)
                result = std::max(result, accessor(&tid_record));
            return result;
        }}

    public:
        static transaction_log& get() noexcept
        {{
            static transaction_log singleton;
            return singleton;
        }}

        inline void publish(thread_record record) noexcept
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
    param = {
        'counter' : '++{MEM_NAME}',
        'max'     : '{MEM_NAME} = std::max({MEM_NAME}, {PARAMS})',
        'sum'     : '{MEM_NAME} += {PARAMS}',
    }
    return '\n'.join([define + '/**/'
                      for define in get_macro_defines(stats, stats_kinds, macro_prefix)])

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

def get_contents(stats, compound_stats, op, operands):
    casted = map_get_mem_or_func_call(operands, stats, compound_stats)
    if op == '/':
        casted[-1] = 'float(%s)' % casted[-1]
    return (' ' + op + ' ').join(casted)

def get_thread_record_mem_fun(compound_stat, stats, compound_stats, compound_stats_kinds):
    _FORMAT_STRING = '''auto {NAME} const noexcept {{ return {CONTENTS}; }}'''
    return _FORMAT_STRING.format(
        NAME     = get_mem_fun_for_stat(compound_stat),
        CONTENTS = get_contents(stats, compound_stats, *compound_stats_kinds[compound_stat]),
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
                        
def get_transaction_log_mem_fun_contents(stat, stats, stats_kinds, compound_stats_kinds):
    if stat in stats:
        if stats_kinds[stat] == 'counter' or stats_kinds[stat] == 'sum':
            return 'total_count(&thread_record::%s)' % get_mem_name(stat)
        elif stats_kinds[stat] == 'max':
            return 'this->max(&thread_record::%s)' % get_mem_name(stat)
        else:
            assert(false)
    
    op = compound_stats_kinds[stat][0]
    operands = map(get_mem_fun_for_stat, compound_stats_kinds[stat][1])
    if op == '/':
        operands[-1] = 'float(%s)' % operands[-1]
    return (' ' + op + ' ').join(operands)

def get_transaction_log_mem_fun(stat, stats, stats_kinds, compound_stats_kinds):
    _FORMAT_STRING = '''auto {NAME} const noexcept {{ return {CONTENTS}; }}'''
    return _FORMAT_STRING.format(
        NAME     = get_mem_fun_for_stat(stat),
        CONTENTS = get_transaction_log_mem_fun_contents(stat,
                                                        stats,
                                                        stats_kinds,
                                                        compound_stats_kinds),
    )

def get_transaction_log_mem_funs(stats,
                                 compound_stats,
                                 stats_kinds,
                                 compound_stats_kinds):
    return '\n'.join([get_transaction_log_mem_fun(stat, stats, stats_kinds, compound_stats_kinds)
                      for stat in stats] +
                     [get_transaction_log_mem_fun(compound_stat,
                                                  stats,
                                                  stats_kinds,
                                                  compound_stats_kinds)
                      for compound_stat in compound_stats])

def get_transaction_log_stream_output(all_stats):
    names = add_trailing_whitespace([get_pretty_name(s) + ':' for s in all_stats])
    values = add_trailing_whitespace([get_mem_fun_for_stat(s)
                                      for s in all_stats])
    return '\n'.join(['<< "' + name + '" << ' + value + ' << \'\\n\''
                        for name, value in zip(names, values)])

def get_includes():
    return indent('''#include <lstm/detail/compiler.hpp>
#include <lstm/detail/namespace_macros.hpp>''')

def gen_stats():
    stats_kinds = {
        'user failures'    : 'counter',
        'failures'         : 'counter',
        'successes'        : 'counter',
        'bloom collisions' : 'counter',
        'bloom successes'  : 'counter',
        'quiesces'         : 'counter',
        'max write size'   : 'max',
        'max read size'    : 'max',
        'reads'            : 'sum',
        'writes'           : 'sum',
    }

    stats = [
        'reads',
        'writes',
        'max read size',
        'max write size',
        'quiesces',
        'user failures',
        'failures',
        'successes',
        'bloom collisions',
        'bloom successes',
    ]
    assert(sorted(stats) == sorted(stats_kinds.keys()))

    compound_stats_kinds = {
        'transactions'          : ['+', ['failures', 'successes']],
        'internal failures'     : ['-', ['failures', 'user failures']],
        'success rate'          : ['/', ['successes', 'transactions']],
        'failure rate'          : ['/', ['failures', 'transactions']],
        'internal failure rate' : ['/', ['internal failures', 'transactions']],
        'user failure rate'     : ['/', ['user failures', 'transactions']],
        'bloom checks'          : ['+', ['bloom successes', 'bloom collisions']],
        'bloom collision rate'  : ['/', ['bloom collisions', 'bloom checks']],
        'quiesce rate'          : ['/', ['quiesces', 'transactions']],
        'average read size'     : ['/', ['reads', 'transactions']],
        'average write size'    : ['/', ['writes', 'transactions']],
    }

    compound_stats = [
        'internal failures',
        'transactions',
        'success rate',
        'failure rate',
        'internal failure rate',
        'user failure rate',
        'bloom checks',
        'bloom collision rate',
        'quiesce rate',
        'average read size',
        'average write size',
    ]
    assert(sorted(compound_stats) == sorted(compound_stats_kinds.keys()))

    all_stats = [
        'transactions',
        'success rate',
        'failure rate',
        'quiesce rate',
        'average write size',
        'average read size',
        'max write size',
        'max read size',
        'bloom collision rate',
        'reads',
        'writes',
        'quiesces',
        'successes',
        'failures',
        'internal failure rate',
        'user failure rate',
        'internal failures',
        'user failures',
        'bloom collisions',
        'bloom successes',
        'bloom checks',
    ]
    assert(sorted(all_stats) == sorted(compound_stats + stats))
    _MACRO_PREFIX = 'LSTM_LOG_'
    _NS_ACCESS = 'lstm::detail::'

    print _STATS_TEMPLATE.format(
        INCLUDE_GUARD = 'LSTM_DETAIL_TRANSACTION_LOG_HPP',
        MACRO_PREFIX = _MACRO_PREFIX,
        INCLUDES = get_includes(),
        NAMESPACE_BEGIN = 'LSTM_DETAIL_BEGIN',
        NAMESPACE_END = 'LSTM_DETAIL_END',
        NS_ACCESS = _NS_ACCESS,
        MACROS_ON = indent(get_macros_on(stats, stats_kinds, _NS_ACCESS, _MACRO_PREFIX), 1),
        MACROS_OFF = indent(get_macros_off(stats, stats_kinds, _MACRO_PREFIX), 1),
        THREAD_RECORD_MEMBERS = indent(get_thread_record_mems(stats, stats_kinds), 2),
        THREAD_RECORD_MEMBER_FUNCTIONS = indent(get_thread_record_mem_funs(stats,
                                                                           compound_stats,
                                                                           compound_stats_kinds),
                                                2),
        THREAD_RECORD_STREAM_OUTPUT = indent(get_thread_record_stream_output(all_stats,
                                                                             stats,
                                                                             compound_stats), 4),
        TRANSACTION_LOG_MEMBER_FUNCTIONS = indent(get_transaction_log_mem_funs(stats,
                                                                               compound_stats,
                                                                               stats_kinds,
                                                                               compound_stats_kinds), 2),
        TRANSACTION_LOG_STREAM_OUTPUT = indent(get_transaction_log_stream_output(all_stats), 4),
    )

def main():
    gen_stats()
    
if __name__ == '__main__':
    main()