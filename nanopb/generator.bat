cd /d %~dp0

nanopb-0.3.9.4-windows-x86\generator-bin\protoc.exe --nanopb_out=. RelayConfig.proto
copy RelayConfig.pb.h ..\include /y
copy RelayConfig.pb.c ..\src /y
del RelayConfig.pb.h
del RelayConfig.pb.c
