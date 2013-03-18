{
  'targets': [
    {
      'target_name': 'winston-tee',
      'type': 'executable',
      'dependencies': [
        'uv/uv.gyp:libuv',
      ],
      'defines': [
      ],
      'shared_libraries': [
        'curl/lib/libcurl'
      ],
      'include_dirs': [
        'uv/include',
        'curl/include/curl',
        'commander.c/src'
        'src',
      ],
      'sources': [
        'test.c',
        'uv_queue.c',
      ],
      'conditions': [
      ]
    }
  ]
}