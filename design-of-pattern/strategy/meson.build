strategy_sources = files(
    'bubble_sort.cpp',
    'quick_sort.cpp',
    'merge_sort.cpp',
    'sort_context.cpp',
)

strategy_inc = include_directories('.')

executable('strategy',
    ['main.cpp'] + strategy_sources,
    dependencies: [fmt_dep],
    include_directories: strategy_inc,
)