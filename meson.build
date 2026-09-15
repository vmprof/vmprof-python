project('vmprof', 'c',
  version: '0.5.1',
  license: 'MIT',
  meson_version: '>=1.1.0',
)

py = import('python').find_installation(pure: false)
py_dep = py.dependency()
cc = meson.get_compiler('c')

# PyPy ships _vmprof as a built-in module, so the PyPy wheel is pure python.
is_pypy = run_command(py, '-c',
  'import sys; print("__pypy__" in sys.builtin_module_names)',
  check: true,
).stdout().strip() == 'True'

# The pure python packages. The test packages are not installed.
python_dir = py.get_install_dir(pure: is_pypy)
install_subdir('vmprof',
  install_dir: python_dir,
  exclude_directories: ['test', '__pycache__'],
)
install_subdir('jitlog',
  install_dir: python_dir,
  exclude_directories: ['test', '__pycache__'],
)
install_subdir('vmshare',
  install_dir: python_dir,
  exclude_directories: ['__pycache__'],
)

if not is_pypy
  system = host_machine.system()

  sources = files(
    'src/_vmprof.c',
    'src/machine.c',
    'src/compat.c',
    'src/vmp_stack.c',
    'src/vmprof_common.c',
    'src/vmprof_memory.c',
  )
  c_args = []
  deps = [py_dep]
  # libbacktrace ships a pre-generated config.h, so it is built as plain
  # sources without running its configure script.
  inc = include_directories('src', 'src/libbacktrace')

  if system == 'windows'
    sources += files('src/vmprof_win.c')
    c_args += ['-DVMPROF_WINDOWS=1']
  elif system == 'darwin'
    sources += files(
      'src/symboltable.c',
      'src/vmprof_unix.c',
      'src/vmprof_mt.c',
    )
    c_args += ['-DVMPROF_APPLE=1', '-DVMPROF_UNIX=1']
  elif system == 'linux' or system == 'freebsd'
    sources += files(
      'src/symboltable.c',
      'src/vmprof_unix.c',
      'src/vmprof_mt.c',
      'src/libbacktrace/backtrace.c',
      'src/libbacktrace/state.c',
      'src/libbacktrace/elf.c',
      'src/libbacktrace/dwarf.c',
      'src/libbacktrace/fileline.c',
      'src/libbacktrace/mmap.c',
      'src/libbacktrace/mmapio.c',
      'src/libbacktrace/posix.c',
      'src/libbacktrace/sort.c',
    )
    c_args += ['-DVMPROF_UNIX=1']
    unwind_dep = dependency('libunwind', required: false)
    if not unwind_dep.found()
      unwind_dep = cc.find_library('unwind')
    endif
    deps += unwind_dep
    if system == 'linux'
      c_args += ['-DVMPROF_LINUX=1']
      deps += cc.find_library('dl', required: false)
    else
      c_args += ['-DVMPROF_BSD=1']
      inc = [inc, include_directories('/usr/local/include')]
    endif
  else
    error('platform "' + system + '" is not supported')
  endif

  if system != 'windows'
    c_args += cc.get_supported_arguments('-Wno-unused')
  endif

  if py.language_version().version_compare('>=3.11')
    sources += files('src/populate_frames.c')
  endif

  py.extension_module('_vmprof',
    sources,
    c_args: c_args,
    include_directories: inc,
    dependencies: deps,
    install: true,
  )
endif
