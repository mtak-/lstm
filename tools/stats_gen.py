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

include_guard = 'LSTM_DETAIL_PERF_STATS_HPP'

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
        stat_output_ordering = stat_output_ordering,
        stats_member_ordering = stats_member_ordering,
        compound_stats_member_func_ordering = compound_stats_member_func_ordering,
    ))
    
if __name__ == '__main__':
    main()