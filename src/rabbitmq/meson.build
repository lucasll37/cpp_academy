common_deps = [fmt_dep, boost_dep, amqpcpp_dep]

executable(
  'rabbitMQ_producer',
  files('./producer.cpp'),
  dependencies: common_deps,
  install: true
)

executable(
  'rabbitMQ_consumer',
  files('./consumer.cpp'),
  dependencies: common_deps,
  install: true
)