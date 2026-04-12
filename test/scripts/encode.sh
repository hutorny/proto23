#!/usr/bin/bash

numeric() {
  echo -n "  {{$1$2}," '"'
  echo "v:$1" | protoc -I ../proto --encode=tests.numeric.$3 numeric.proto | hexdump -v -e '"\\" "x" 1/1 "%02X"'
  echo '"sv },'
}

pairs() {
  echo -n "  {{$1, $2$4}," '"'
  echo "k:$1;v:$2" | protoc -I ../proto --encode=tests.pairs.$3 pairs.proto | hexdump -v -e '"\\" "x" 1/1 "%02X"'
  echo '"sv },'
}

list() {
  for v in "${@:3}"; do
        echo -n "$1$v$2"
  done
}

begin() {
  echo "const Item<$1> $1_tests[] {"  
}

ending() {
  echo '};'  
}


vectors() {
  echo -n '  {{{'
  list '' $2 ${@:3} 
  echo -n '}}, "'
  list 'v:' ';' ${@:3} | protoc -I ../proto --encode=tests.vectors.$1 vectors.proto | hexdump -v -e '"\\" "x" 1/1 "%02X"'
  echo '"sv },'  
}

messages() {
  echo -n "  {$3," '"'
  echo "$2" | protoc -I ../proto --encode=tests.messages.$1 messages.proto | hexdump -v -e '"\\" "x" 1/1 "%02X"'
  echo '"sv },'
}

strings() {
  echo -n '  {{"'${3:-$2}'"}, "'
  echo 'v:"'$2'"' | protoc -I ../proto --encode=tests.strings.$1 strings.proto | hexdump -v -e '"\\" "x" 1/1 "%02X"'  
  echo '"sv },'
}

begin Map
  messages Map 'v:1;dict:{key:1;value:"OK";};dict:{key:2;value:"Good"};' '{1, {{1, "OK"},{2,"Good"}}}'
ending

begin UnorderedMap
  messages Map 'v:1;dict:{key:1;value:"OK";};dict:{key:2;value:"Good"};' '{1, {{1, "OK"},{2,"Good"}}}'
ending

begin Optional
  messages Outer 'v:1;inner:{v:2;status:OK};' '{Inner{Status::OK, 2},1}'
  messages Outer 'v:1' '{{},1}'
  messages Outer 'v:8388607;inner:{v:65535;status:NOTOK};' '{Inner{Status::NOTOK, 65535},8388607}'
  messages Outer 'inner:{v:65535;status:NOTOK};' '{Inner{Status::NOTOK, 65535}}'
ending

begin Union
  messages OneOf 'v:1;value:1.0;' '{1, {.value=1.0}}'
  messages OneOf 'v:2;count:2;' '{2, {.count=2}}'
ending

begin Variant
  messages OneOf 'v:1;value:1.0;' '{1, {1.0}}'
  messages OneOf 'v:2;count:2;' '{2, {2}}'
ending

begin String
  strings String 'simple string'
  strings String 'solidus \\\\\\\\\\\\\\\\'
ending

begin Chars
  strings String 'simple chars'
  strings String 'solidus characterus \\\\\\\\\\\\\\\\'
ending

begin CharArray
  strings String 'simple chars'
  strings String 'solidus characterus \\\\\\\\\\\\\\\\'
ending

begin ExcessiveChars
  strings Excessive 'string with excessive length' 'string with exc'
ending

begin ExcessiveArray
  strings Excessive 'array with excessive length' 'array with exc'
ending

begin Outer
  messages Outer 'v:1;inner:{v:2;status:OK};' '{{Status::OK, 2},1}'
  messages Outer 'v:8388607;inner:{v:65535;status:NOTOK};' '{{Status::NOTOK, 65535},8388607}'
ending


begin Combined
  messages Combined 'v:1;inner:{v:2;status:OK};outer:{v:3; inner:{v:4;status:OK};};local:{v:5;text:"local text"};' '{{Status::OK, 2},{{Status::OK, 4},3},{5,"local text"}, 1}'
  messages Combined 'v:2;outer:{v:65535; inner:{v:8388607;status:OK};};local:{v:-15};' '{{},{{Status::OK, 8388607},65535},{-15}, 2}'
ending

begin Bool_u 
vectors Bool_u ',' true true false true
ending

begin Int_32u 
vectors Int_32u '_i32,' 10 0 11 12 -13
ending

begin SInt_32u 
vectors SInt_32u ',' 20 0 21 22 -23
ending

begin UInt_32u 
vectors UInt_32u '_u32,' 30 31 32 0 33
ending

begin Fixed_32u 
vectors Fixed_32u '_f32,' 40 41 42 0 43
ending

begin SFixed_32u 
vectors SFixed_32u '_sf32,' 50 0 51 52 -53
ending

begin Int_64u 
vectors Int_64u '_i64,' 60 0 61 62 -63
ending

begin SInt_64u 
vectors SInt_64u '_s64,' 71 -72 0 73 -74
ending

begin UInt_64u 
vectors UInt_64u '_u64,' 81 82 0 83 84
ending

begin Fixed_64u 
vectors Fixed_64u '_f64,' 91 92 0 93 94
ending

begin SFixed_64u 
vectors SFixed_64u '_sf64,' 95 0 96 -97 98
ending

begin Double_u 
vectors Double_u ',' 2.2 9.6E12 -3.97E-100 0 1E9
ending

begin Float_u 
vectors Float_u ',' 1.1 9.6E-12 0 3.97E-14 -1E6
ending

begin Bool_v 
vectors Bool_v ',' true false true false true
ending

begin Int_32v 
vectors Int_32v '_i32,' 10 0 11 12 -13
ending

begin SInt_32v 
vectors SInt_32v ',' 20 0 21 22 -23
ending

begin UInt_32v 
vectors UInt_32v '_u32,' 30 31 32 0 33
ending

begin Fixed_32v 
vectors Fixed_32v '_f32,' 40 41 42 0 43
ending

begin SFixed_32v 
vectors SFixed_32v '_sf32,' 50 0 51 52 -53
ending

begin Int_64v 
vectors Int_64v '_i64,' 60 0 61 62 -63
ending

begin SInt_64v 
vectors SInt_64v '_s64,' 71 -72 0 73 -74
ending

begin UInt_64v 
vectors UInt_64v '_u64,' 81 82 0 83 84
ending

begin Fixed_64v 
vectors Fixed_64v '_f64,' 91 92 0 93 94
ending

begin SFixed_64v 
vectors SFixed_64v '_sf64,' 95 0 96 -97 98
ending

begin Double_v 
vectors Double_v ',' 2.2 9.6E12 -3.97E-100 0 1E9
ending

begin Float_v 
vectors Float_v ',' 1.1 9.6E-12 0 3.97E-14 -1E6
ending

 
begin Bool_SInt
pairs true 10 Bool_SInt
pairs false 8388608 Bool_SInt
ending

begin SInt_String
pairs 12 '"twelve"' SInt_String
pairs 2147483647 '"biggest number"' SInt_String
ending

begin UInt_Fixed
pairs 16 16 UInt_Fixed _f32
pairs 4294967295 4294967295 UInt_Fixed _f32
ending

begin Double_SFixed
pairs 0.3 16 Double_SFixed _sf32
pairs 4.1e12 -16 Double_SFixed _sf32
ending

begin Bool_
numeric true "" Bool_
numeric false "" Bool_
ending

begin SInt_32
for v in -2147483647 -8388608 -8388607 -65536 -65535 -32768 -32767 -257 -256 -129 -128 -17 -16 -2 -1 0 1 2 15 16 127 128 255 256 32767 32768 65535 65536 8388607 8388608 2147483647; do 
  numeric $v _s32 SInt_32 
done
ending

begin Int_32
for v in -2147483647 -8388608 -8388607 -65536 -65535 -32768 -32767 -257 -256 -129 -128 -17 -16 -2 -1 0 1 2 15 16 127 128 255 256 32767 32768 65535 65536 8388607 8388608 2147483647; do 
 numeric $v _i32 Int_32
done
ending

begin UInt_32
for v in 0 1 2 15 16 127 128 255 32767 32768 65535 65536 8388607 8388608 2147483647 2147483648 4294967295; do
 numeric $v _u32 UInt_32
done
ending

begin Fixed_32
for v in 0 1 2 15 16 127 128 255 32767 32768 65535 65536 8388607 8388608 2147483647 2147483648 4294967295; do
 numeric $v _f32 Fixed_32
done
ending

begin SFixed_32
for v in -2147483647 -8388608 -8388607 -65536 -65535 -32768 -32767 -257 -256 -129 -128 -17 -16 -2 -1 0 1 2 15 16 127 128 255 256 32767 32768 65535 65536 8388607 8388608 2147483647; do 
 numeric $v _sf32 SFixed_32
done
ending

begin SInt_64
for v in -1099511627776 -2147483647 -8388608 -8388607 -65536 -65535 -32768 -32767 -257 -256 -129 -128 -17 -16 -2 -1 0 1 2 15 16 127 128 255 256 32767 32768 65535 65536 8388607 8388608 2147483647 1099511627776; do 
  numeric $v _s64 SInt_64 
done
ending

begin Int_64
for v in -1099511627776 -2147483647 -8388608 -8388607 -65536 -65535 -32768 -32767 -257 -256 -129 -128 -17 -16 -2 -1 0 1 2 15 16 127 128 255 256 32767 32768 65535 65536 8388607 8388608 2147483647 1099511627776; do 
 numeric $v _i64 Int_64
done
ending

begin UInt_64
for v in 0 1 2 15 16 127 128 255 32767 32768 65535 65536 8388607 8388608 2147483647 2147483648 4294967295 1099511627776; do
 numeric $v _u64 UInt_64
done
ending

begin Fixed_64
for v in 0 1 2 15 16 127 128 255 32767 32768 65535 65536 8388607 8388608 2147483647 2147483648 4294967295 4294967296 1099511627776; do
 numeric $v _f64 Fixed_64
done
ending

begin SFixed_64
for v in -1099511627776 -2147483647 -8388608 -8388607 -65536 -65535 -32768 -32767 -257 -256 -129 -128 -17 -16 -2 -1 0 1 2 15 16 127 128 255 256 32767 32768 65535 65536 8388607 8388608 2147483647 1099511627776; do 
 numeric $v _sf64 SFixed_64
done
ending

begin Double_
for v in  -1e+99 -2.99792e+08 -0.01 -1.001e-24 1.e-99 0 1.e-99 0.01 1 1.1 99.99 7.01e+14 1.e99; do  
 numeric $v "" Double_
done
ending

begin Float_
for v in  -2.99792e+08 -0.01 -1.001e-24 0 1.001e-24 0.01 1 1.1 99.99 7.01e+14; do
 numeric $v "" Float_
done
ending
