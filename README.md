# ndperf
This is the ndp benchmark tool in accordance with [RFC8161](https://tools.ietf.org/html/rfc8161).

Requirements
------------

* libnl-3-dev
* libnl-route-3-dev

Build
------------
```
$ git clone git@github.com:teddyyy/ndperf.git  
$ cd ndperf  
$ make
```

Usage
-------
* Baseline test
```
$ sudo ./ndperf -B -i enp0s3 -r enp0s8 -s 2001:2:0:0::1 -d 2001:2:0:1::1 -p 64
```

* Scaling test
```
$ sudo ./ndperf -S -n 1000 -i enp0s3 -r enp0s8 -s 2001:2:0:0::1 -d 2001:2:0:1::1 -p 64
```

* Topology
```
  +---------------+
  |               |                                +---------------+
  |    Tester     |                                |               |
  |        (2001:2:0:0::1) ---------------> (2001:2:0:0::2)        |
  |               |                                |      DUT      |
  |               |                                |               |
  |        (2001:2:0:1::2, <--------------- (2001:2:0:1::1)        |
  |         2001:2:0:1::3,                         |               |
  |         2001:2:0:1::4...)                      +---------------+
  |               |
  +---------------+
```

License
------------
See [LICENSE](LICENSE)
