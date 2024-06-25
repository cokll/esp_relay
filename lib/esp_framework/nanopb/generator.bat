cd /d %~dp0

D:\Project\Smarthome\归档\esp\nanopb\generator-bin\protoc.exe --nanopb_out=. GlobalConfig.proto
copy GlobalConfig.pb.h ..\include /y
copy GlobalConfig.pb.c ..\src /y
del GlobalConfig.pb.h
del GlobalConfig.pb.c
