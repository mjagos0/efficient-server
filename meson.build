project('cocontest-turkeybox', ['c', 'cpp'],
  version: '0.1',
  default_options: [
    'warning_level=3',
    'buildtype=debug'
  ]
)

protobuf_dep = dependency('protobuf')

add_project_arguments(
  '-std=c++23',
  '-O3',
  '-march=native',
  '-pthread',
  '-g',
  '-Wall',
  # '-fsanitize=address',
  # '-fsanitize=thread',
  language: 'cpp'
)

add_project_link_arguments(
  # '-fsanitize=address',
  # '-fsanitize=thread',
  language: 'cpp'
)

# Main app sources
sources = [
  'src/main.cpp',
  'src/schema.pb.cc'
]

exe = executable('main', sources,
  install : true,
  dependencies : [protobuf_dep],
  # cpp_args : [
  #  '-D_DEBUG'
  # ]
)
