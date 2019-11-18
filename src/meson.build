


c_sources = run_command('read_c_sources.sh')
list_c_sources = files(c_sources.stdout().strip().split('\n'))


h_headers = run_command('read_headers.sh')
list_h_headers = h_headers.stdout().strip().split('\n')

install_headers(list_h_headers)

add_project_arguments('-D_LARGEFILE64_SOURCE', language : 'c')
add_project_arguments('-D_REENTRANT', language : 'c')
add_project_arguments('-D_FILE_OFFSET_BITS=64', language : 'c')
add_project_arguments('-D_GNU_SOURCE', language : 'c')
add_project_arguments('-DHAVE_CONFIG_H', language : 'c')


executable('fsarchiver', dependencies : deps, link_args : links, sources: list_c_sources, include_directories : configuration_inc)
