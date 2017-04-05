#!/usr/bin/python

import stats_gen_lib as sgl
import os

stats = {
    'user failures'         : 'counter',
    'failures'              : 'counter',
    'successes'             : 'counter',
    'bloom collisions'      : 'counter',
    'bloom successes'       : 'counter',
    'quiesces'              : 'counter',
    'backoffs'              : 'counter',
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
    'backoff rate'          : {'op' : '/', 'operands' : ['backoffs', 'transactions']},
    'average read size'     : {'op' : '/', 'operands' : ['reads', 'transactions']},
    'average write size'    : {'op' : '/', 'operands' : ['writes', 'transactions']},
}

stats_member_ordering = [
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

compound_stats_member_func_ordering = [
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

stat_output_ordering = [
    'transactions',
    'success rate',
    'failure rate',
    'quiesce rate',
    'backoff rate',
    'average write size',
    'average read size',
    'max write size',
    'max read size',
    'bloom collision rate',
    'reads',
    'writes',
    'quiesces',
    'backoffs',
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

include_guard = 'LSTM_DETAIL_PERF_STATS_HPP'

_STATSD_TEMPLATE = '''#ifndef LSTM_TEST_STATSD_CLIENT_HPP
#define LSTM_TEST_STATSD_CLIENT_HPP

#include <lstm/lstm.hpp>

#if defined(LSTM_STATSD_AVAILABLE) && defined(LSTM_PERF_STATS_ON) && defined(NDEBUG)

extern "C" {{
#include <statsd-client.h>
}}

struct statsd_client
{{
private:
    statsd_link* link;

    statsd_client()
    {{
        link = statsd_init("127.0.0.1", 8125);
        LSTM_ASSERT(link);
    }}
    ~statsd_client() {{ statsd_finalize(link); }}

    static statsd_client& get()
    {{
        static statsd_client client;
        return client;
    }}

public:
    static void dump_log()
    {{
        statsd_link*              link  = get().link;
        lstm::detail::perf_stats& stats = lstm::detail::perf_stats::get();
{STATSD_OUTPUT}
    }}
}};

#else /* defined(LSTM_STATSD_AVAILABLE) && defined(LSTM_PERF_STATS_ON) && defined(NDEBUG) */

struct statsd_client
{{
    static constexpr void dump_log() noexcept {{}}
}};

#endif /* defined(LSTM_STATSD_AVAILABLE) && defined(LSTM_PERF_STATS_ON) && defined(NDEBUG) */

#endif /* LSTM_TEST_STATSD_CLIENT_HPP */'''

def main():
    p = os.path.realpath(os.path.join(os.path.dirname(__file__), '../include/lstm/detail/perf_stats.hpp'))
    f = open(p, 'w')
    f.write(sgl.gen_stats(
        stats,
        include_guard,
        class_name = 'perf_stats',
        macro_prefix = 'LSTM_PERF_STATS_',
        namespace_access = 'lstm::detail::',
        namespace_begin = 'LSTM_DETAIL_BEGIN',
        namespace_end = 'LSTM_DETAIL_END',
        includes = '''#include <lstm/detail/compiler.hpp>
#include <lstm/detail/namespace_macros.hpp>''',
        stat_output_ordering = stat_output_ordering,
        stats_member_ordering = stats_member_ordering,
        compound_stats_member_func_ordering = compound_stats_member_func_ordering,
    ))
    f.close()
    p = os.path.realpath(os.path.join(os.path.dirname(__file__), '../test/statsd_client.hpp'))
    f = open(p, 'w')
    f.write(_STATSD_TEMPLATE.format(STATSD_OUTPUT = sgl.gen_statsd_output(
        stats,
        include_guard,
        class_name = 'perf_stats',
        macro_prefix = 'LSTM_PERF_STATS_',
        namespace_access = 'lstm::detail::',
        namespace_begin = 'LSTM_DETAIL_BEGIN',
        namespace_end = 'LSTM_DETAIL_END',
        stat_output_ordering = stat_output_ordering,
        stats_member_ordering = stats_member_ordering,
        compound_stats_member_func_ordering = compound_stats_member_func_ordering,
    )))
    f.close()
    
if __name__ == '__main__':
    main()