{
  "targets": [
    {
      "target_name": "posix",
      "sources": [ "src/posix.cc" ],
      "link_settings": {
        "include_dirs" : [ "<!(node -e \"require('nan')\")" ]
      }
    }
  ]
}
