#!/usr/bin/python

from string import Formatter

_STATS_TEMPLATE = '''#ifndef LSTM_DETAIL_TRANSACTION_LOG_HPP
#define LSTM_DETAIL_TRANSACTION_LOG_HPP

// clang-format off
#ifdef LSTM_LOG_TRANSACTIONS
    #include <lstm/detail/compiler.hpp>
    #include <lstm/detail/namespace_macros.hpp>

    #include <iomanip>
    #include <sstream>
    #include <string>
    #include <vector>
    
{MACROS_ON}
    #define LSTM_LOG_PUBLISH_RECORD()                                                              \\
        do {{                                                                                       \\
            lstm::detail::transaction_log::get().publish(lstm::detail::tls_record());              \\
            lstm::detail::tls_record() = {{}};                                                       \\
        }} while(0)
    /**/
    #define LSTM_LOG_CLEAR() lstm::detail::transaction_log::get().clear()
    #ifndef LSTM_LOG_DUMP
        #include <iostream>
        #define LSTM_LOG_DUMP() (std::cout << lstm::detail::transaction_log::get().results())
    #endif /* LSTM_LOG_DUMP */
#else
{MACROS_OFF}
    #define LSTM_LOG_PUBLISH_RECORD()                               /**/
    #define LSTM_LOG_CLEAR()                                        /**/
    #ifndef LSTM_LOG_DUMP
        #define LSTM_LOG_DUMP()                                     /**/
    #endif /* LSTM_LOG_DUMP */
#endif /* LSTM_LOG_TRANSACTIONS */
// clang-format on

#ifdef LSTM_LOG_TRANSACTIONS

LSTM_DETAIL_BEGIN
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
LSTM_DETAIL_END

#endif /* LSTM_LOG_TRANSACTIONS */

#endif /* LSTM_DETAIL_TRANSACTION_LOG_HPP */'''

_MACRO_PREFIX = 'LSTM_LOG_'

stats_kinds = {
    'user failures'    : 'counter',
    'failures'         : 'counter',
    'successes'        : 'counter',
    'bloom collisions' : 'counter',
    'bloom successes'  : 'counter',
    'quiesces'          : 'counter',
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

types = {
    'counter' : 'std::uint64_t',
    'max'     : 'std::uint64_t',
    'sum'     : 'std::uint64_t',
}

# MACROS_ON # done
# MACROS_OFF # done
# THREAD_RECORD_MEMBERS # done
# THREAD_RECORD_MEMBER_FUNCTIONS # done
# THREAD_RECORD_STREAM_OUTPUT #done
# TRANSACTION_LOG_MEMBER_FUNCTIONS
# TRANSACTION_LOG_STREAM_OUTPUT

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

def get_macro_name(stat):
    return _MACRO_PREFIX + stat.upper().replace(' ', '_')
    
def get_macro_params(stat):
    param = {
        'counter' : '',
        'max'     : 'amt',
        'sum'     : 'amt',
    }
    return param[stats_kinds[stat]]
    
def get_macro_define(stat):
    return '#define %s(%s)' % (get_macro_name(stat), get_macro_params(stat))

def add_trailing_whitespace(strings):
    max_length = max([len(x) for x in strings])
    return ['{0: <{1}}'.format(x, max_length + 1) for x in strings]
    
    
def get_macro_defines(the_stats):
    return add_trailing_whitespace([get_macro_define(stat) for stat in the_stats])
    
def get_macro_expansion_on(stat):
    param = {
        'counter' : '++lstm::detail::tls_record().{MEM_NAME}',
        'max'     : 'lstm::detail::tls_record().{MEM_NAME} = std::max(lstm::detail::tls_record().{MEM_NAME}, static_cast<std::uint64_t>({PARAMS}))',
        'sum'     : 'lstm::detail::tls_record().{MEM_NAME} += {PARAMS}',
    }
    return param[stats_kinds[stat]].format(MEM_NAME = get_mem_name(stat), PARAMS = get_macro_params(stat))

def get_macros_on():
    param = {
        'counter' : '++{MEM_NAME}',
        'max'     : '{MEM_NAME} = std::max({MEM_NAME}, {PARAMS})',
        'sum'     : '{MEM_NAME} += {PARAMS}',
    }
    return '\n'.join([define + get_macro_expansion_on(stat) for stat, define in zip(stats, get_macro_defines(stats))])

def get_macros_off():
    param = {
        'counter' : '++{MEM_NAME}',
        'max'     : '{MEM_NAME} = std::max({MEM_NAME}, {PARAMS})',
        'sum'     : '{MEM_NAME} += {PARAMS}',
    }
    return '\n'.join([define + '/**/' for define in get_macro_defines(stats)])

def get_thread_record_mems():
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

def get_thread_record_mem_fun(compound_stat):
    _FORMAT_STRING = '''auto {NAME} const noexcept {{ return {CONTENTS}; }}'''
    return _FORMAT_STRING.format(
        NAME     = get_mem_fun_for_stat(compound_stat),
        CONTENTS = get_contents(stats, compound_stats, *compound_stats_kinds[compound_stat]),
    )

def get_thread_record_mem_funs():
    return '\n'.join([get_thread_record_mem_fun(compound_stat) for compound_stat in compound_stats])

def get_thread_record_stream_output():
    names = add_trailing_whitespace([get_pretty_name(s) + ':' for s in all_stats])
    values = add_trailing_whitespace([get_mem_or_func_call(s, stats, compound_stats) for s in all_stats])
    return '\n'.join(['<< "    ' + name + '" << ' + value + ' << \'\\n\''
                        for name, value in zip(names, values)])
                        
def get_transaction_log_mem_fun_contents(stat):
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

def get_transaction_log_mem_fun(stat):
    _FORMAT_STRING = '''auto {NAME} const noexcept {{ return {CONTENTS}; }}'''
    return _FORMAT_STRING.format(
        NAME     = get_mem_fun_for_stat(stat),
        CONTENTS = get_transaction_log_mem_fun_contents(stat),
    )

def get_transaction_log_mem_funs():
    return '\n'.join([get_transaction_log_mem_fun(stat) for stat in stats] +
                     [get_transaction_log_mem_fun(compound_stat)
                      for compound_stat in compound_stats])

def get_transaction_log_stream_output():
    names = add_trailing_whitespace([get_pretty_name(s) + ':' for s in all_stats])
    values = add_trailing_whitespace([get_mem_fun_for_stat(s)
                                      for s in all_stats])
    return '\n'.join(['<< "' + name + '" << ' + value + ' << \'\\n\''
                        for name, value in zip(names, values)])

# MACROS_ON # done
# MACROS_OFF # done
# THREAD_RECORD_MEMBERS # done
# THREAD_RECORD_MEMBER_FUNCTIONS # done
# THREAD_RECORD_STREAM_OUTPUT #done
# TRANSACTION_LOG_MEMBER_FUNCTIONS #done
# TRANSACTION_LOG_STREAM_OUTPUT

def gen_stats():
    print _STATS_TEMPLATE.format(
        MACROS_ON = indent(get_macros_on(), 1),
        MACROS_OFF = indent(get_macros_off(), 1),
        THREAD_RECORD_MEMBERS = indent(get_thread_record_mems(), 2),
        THREAD_RECORD_MEMBER_FUNCTIONS = indent(get_thread_record_mem_funs(), 2),
        THREAD_RECORD_STREAM_OUTPUT = indent(get_thread_record_stream_output(), 4),
        TRANSACTION_LOG_MEMBER_FUNCTIONS = indent(get_transaction_log_mem_funs(), 2),
        TRANSACTION_LOG_STREAM_OUTPUT = indent(get_transaction_log_stream_output(), 4),
    )

def main():
    gen_stats()
    
if __name__ == '__main__':
    main()