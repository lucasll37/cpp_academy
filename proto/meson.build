common_proto_cc = custom_target(
  'common_proto_cc',
  input : ['./common.proto'],
  output : ['@BASENAME@.pb.cc'],
  command : [
    protoc,
    '--proto_path=@CURRENT_SOURCE_DIR@',
    '--cpp_out=@OUTDIR@',
    '@INPUT@',
  ]
)

common_proto_h = custom_target(
  'common_proto_h',
  input : ['./common.proto'],
  output : ['@BASENAME@.pb.h'],
  command : [
    protoc,
    '--proto_path=@CURRENT_SOURCE_DIR@',
    '--cpp_out=@OUTDIR@',
    '@INPUT@',
  ],
  install : true,
  install_dir : get_option('includedir'),
)
