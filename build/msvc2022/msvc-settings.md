# Microsoft Visual C++ 2022 설정

본 문서는 VC++ 프로젝트들에 설정한, 빌드 정보를 요약한 것이다.
```
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
프로젝트별 설명
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
project							    output							  description
----------------------------------------------------
[1] 정적 라이브러리
--------------------
/ifcOutput "D:\E\Study\AI\AutoAgents\projects\JK-PQC\build\msvc2022\x64\Release\\obj\jk-https-client\" /GS /GL /W3 /Gy /Zc:wchar_t /I"D:\openssl\3.5.4\include" /I"..\..\..\src\clients" /I"..\..\..\src\common" /I"..\..\..\_deps\cpp-httplib\include" /I"..\..\..\_deps\liboqs\include" /I"..\..\..\_deps\oqs-provider\include" /Zi /Gm- /O2 /Fd"D:\E\Study\AI\AutoAgents\projects\JK-PQC\build\msvc2022\x64\Release\\obj\jk-https-client\vc143.pdb" /Zc:inline /fp:precise /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "CPPHTTPLIB_OPENSSL_SUPPORT" /D "_UNICODE" /D "UNICODE" /errorReport:prompt /WX- /Zc:forScope /std:c17 /Gd /Oi /MT /std:c++17 /FC /Fa"D:\E\Study\AI\AutoAgents\projects\JK-PQC\build\msvc2022\x64\Release\\obj\jk-https-client\" /EHsc /nologo /Fo"D:\E\Study\AI\AutoAgents\projects\JK-PQC\build\msvc2022\x64\Release\\obj\jk-https-client\" /Fp"D:\E\Study\AI\AutoAgents\projects\JK-PQC\build\msvc2022\x64\Release\\obj\jk-https-client\jk-https-client.pch" /diagnostics:column 
/OUT:"..\..\..\bin\msvc\Release\jk-https-client.exe" /MANIFEST /LTCG:incremental /NXCOMPAT /PDB:"..\..\..\bin\msvc\Release\jk-https-client.pdb" /DYNAMICBASE "libssl.lib" "libcrypto.lib" "cpp-httplib-0.40.0.lib" "oqs.lib" "oqsprovider.lib" "ws2_32.lib" "kernel32.lib" "user32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib" "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib" "odbc32.lib" "odbccp32.lib" /DEBUG /MACHINE:X64 /OPT:REF /INCREMENTAL:NO /PGD:"..\..\..\bin\msvc\Release\jk-https-client.pgd" /SUBSYSTEM:CONSOLE /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /ManifestFile:"D:\E\Study\AI\AutoAgents\projects\JK-PQC\build\msvc2022\x64\Release\\obj\jk-https-client\jk-https-client.exe.intermediate.manifest" /LTCGOUT:"D:\E\Study\AI\AutoAgents\projects\JK-PQC\build\msvc2022\x64\Release\\obj\jk-https-client\jk-https-client.iobj" /OPT:ICF /ERRORREPORT:PROMPT /ILK:"D:\E\Study\AI\AutoAgents\projects\JK-PQC\build\msvc2022\x64\Release\\obj\jk-https-client\jk-https-client.ilk" /NOLOGO /LIBPATH:"D:\openssl\3.5.4\lib" /LIBPATH:"..\..\..\_deps\cpp-httplib\lib\Release" /LIBPATH:"..\..\..\_deps\liboqs\lib" /LIBPATH:"..\..\..\_deps\oqs-provider\lib\Release" /TLBID:1 
--------------------
[2] 정적 어플리케이션
--------------------
jk-mtls-client        jk-mtls-client.exe      TCP + PQC + mTLS 인증 클라이언트
jk-mtls-server        jk-mtls-server.exe      TCP + PQC + mTLS 인증 서버

jk-https-client       jk-https-client.exe     HTTPS + PQC + mTLS 인증 클라이언트
jk-https-server       jk-https-server.exe     HTTPS + PQC + mTLS 인증 서버
jk-https-dashboard    jk-https-dashboard.exe  HTTPS(TLS) 대시보드 서버

jk-pqc-server         jk-pqc-server.exe       HTTPS + PQC + mTLS 인증 서버 (고성능)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
프로젝트별 VC++ 설정
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
project					      output							    pre-define									inc-path		                lib-path							                req-libs						            out-path								              mid-path
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Debug
----------------------------------------------------
clients
--------------------
jk-mtls-client        jk-mtls-client.exe      _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\clients	                                            ws2_32.lib;
                                                                          ..\..\..\src\common         
                                        ..\..\..\_deps\$(Configuration)\liboqs\include    ..\..\..\_deps\$(Configuration)\liboqs\lib        oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\clients;..\..\..\src\common;..\..\..\_deps\$(Configuration)\liboqs\include;..\..\..\_deps\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\$(Configuration)\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);
libssl.lib;libcrypto.lib;oqs.lib;oqsprovider.lib;ws2_32.lib;
----------
jk-https-client       jk-https-client.exe     _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\clients	                                            ws2_32.lib;
                                              CPPHTTPLIB_OPENSSL_SUPPORT  ..\..\..\src\common         
                                        ..\..\..\_deps\$(Configuration)\liboqs\include    ..\..\..\_deps\$(Configuration)\liboqs\lib        oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib
                                        ..\..\..\_deps\cpp-httplib\include                ..\..\..\_deps\cpp-httplib\lib\$(Configuration)   cpp-httplib-0.40.0.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE;CPPHTTPLIB_OPENSSL_SUPPORT
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\clients;..\..\..\src\common;..\..\..\_deps\cpp-httplib\include;..\..\..\_deps\$(Configuration)\liboqs\include;..\..\..\_deps\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\cpp-httplib\lib\$(Configuration);..\..\..\_deps\$(Configuration)\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);
libssl.lib;libcrypto.lib;cpp-httplib-0.40.0.lib;oqs.lib;oqsprovider.lib;ws2_32.lib;
--------------------
servers
--------------------
jk-mtls-server        jk-mtls-server.exe      _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\servers	                                            ws2_32.lib;
                                                                          ..\..\..\src\common         
                                        ..\..\..\_deps\$(Configuration)\liboqs\include    ..\..\..\_deps\$(Configuration)\liboqs\lib        oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\servers;..\..\..\src\common;..\..\..\_deps\$(Configuration)\liboqs\include;..\..\..\_deps\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\$(Configuration)\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);
libssl.lib;libcrypto.lib;oqs.lib;oqsprovider.lib;ws2_32.lib;
----------
jk-https-server       jk-https-server.exe     _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\servers	                                            ws2_32.lib;
                                              CPPHTTPLIB_OPENSSL_SUPPORT  ..\..\..\src\common         
                                        ..\..\..\_deps\$(Configuration)\liboqs\include    ..\..\..\_deps\$(Configuration)\liboqs\lib        oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib
                                        ..\..\..\_deps\cpp-httplib\include                ..\..\..\_deps\cpp-httplib\lib\$(Configuration)   cpp-httplib-0.40.0.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE;CPPHTTPLIB_OPENSSL_SUPPORT
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\servers;..\..\..\src\common;..\..\..\_deps\cpp-httplib\include;..\..\..\_deps\$(Configuration)\liboqs\include;..\..\..\_deps\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\cpp-httplib\lib\$(Configuration);..\..\..\_deps\$(Configuration)\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);
libssl.lib;libcrypto.lib;cpp-httplib-0.40.0.lib;oqs.lib;oqsprovider.lib;ws2_32.lib;
----------
jk-https-dashboard    jk-https-dashboard.exe  _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\servers	                                            ws2_32.lib;
                                              CPPHTTPLIB_OPENSSL_SUPPORT  ..\..\..\src\common
                                        ..\..\..\_deps\$(Configuration)\liboqs\include    ..\..\..\_deps\$(Configuration)\liboqs\lib        oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib
                                        ..\..\..\_deps\cpp-httplib\include                ..\..\..\_deps\cpp-httplib\lib\$(Configuration)   cpp-httplib-0.40.0.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE;CPPHTTPLIB_OPENSSL_SUPPORT
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\servers;..\..\..\src\common;..\..\..\_deps\cpp-httplib\include;..\..\..\_deps\$(Configuration)\liboqs\include;..\..\..\_deps\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\cpp-httplib\lib\$(Configuration);..\..\..\_deps\$(Configuration)\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);
libssl.lib;libcrypto.lib;cpp-httplib-0.40.0.lib;oqs.lib;oqsprovider.lib;ws2_32.lib;
----------
jk-pqc-server         jk-pqc-server.exe       _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\servers	                                            ws2_32.lib;
                                              CPPHTTPLIB_OPENSSL_SUPPORT  ..\..\..\src\common
                                        ..\..\..\_deps\$(Configuration)\liboqs\include    ..\..\..\_deps\$(Configuration)\liboqs\lib        oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib
                                        ..\..\..\_deps\$(Configuration)\libllhttp\include ..\..\..\_deps\$(Configuration)\libllhttp\lib     llhttp.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE;CPPHTTPLIB_OPENSSL_SUPPORT
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\servers;..\..\..\src\common;..\..\..\_deps\$(Configuration)\liboqs\include;..\..\..\_deps\oqs-provider\include;..\..\..\_deps\$(Configuration)\libllhttp\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\$(Configuration)\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);..\..\..\_deps\$(Configuration)\libllhttp\lib;
libssl.lib;libcrypto.lib;oqs.lib;oqsprovider.lib;llhttp.lib;ws2_32.lib;
--------------------
tests
--------------------
test_provider         test_provider.exe       _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE
                                        ..\..\..\_deps\$(Configuration)\liboqs\include    ..\..\..\_deps\$(Configuration)\liboqs\lib        oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE
$(OPENSSL_ROOT_DIR)\include;..\..\..\_deps\$(Configuration)\liboqs\include;..\..\..\_deps\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\$(Configuration)\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);
libssl.lib;libcrypto.lib;oqs.lib;oqsprovider.lib;
----------
test_socketpool       test_socketpool.exe     _CRT_SECURE_NO_WARNINGS     ..\..\..\src\common                                               ws2_32.lib;                     ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE
_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE
..\..\..\src\common;
ws2_32.lib;
----------------------------------------------------
Release
----------------------------------------------------
clients
--------------------
jk-mtls-client        jk-mtls-client.exe      _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\clients	                                            ws2_32.lib;
                                                                          ..\..\..\src\common         
                                        ..\..\..\_deps\liboqs\include                     ..\..\..\_deps\liboqs\lib                         oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\clients;..\..\..\src\common;..\..\..\_deps\liboqs\include;..\..\..\_deps\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);
libssl.lib;libcrypto.lib;oqs.lib;oqsprovider.lib;ws2_32.lib;
----------
jk-https-client       jk-https-client.exe     _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\clients	                                            ws2_32.lib;
                                              CPPHTTPLIB_OPENSSL_SUPPORT  ..\..\..\src\common         
                                        ..\..\..\_deps\liboqs\include                     ..\..\..\_deps\liboqs\lib                         oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib
                                        ..\..\..\_deps\cpp-httplib\include                ..\..\..\_deps\cpp-httplib\lib\$(Configuration)   cpp-httplib-0.40.0.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE;CPPHTTPLIB_OPENSSL_SUPPORT
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\clients;..\..\..\src\common;..\..\..\_deps\cpp-httplib\include;..\..\..\_deps\liboqs\include;..\..\..\_deps\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\cpp-httplib\lib\$(Configuration);..\..\..\_deps\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);
libssl.lib;libcrypto.lib;cpp-httplib-0.40.0.lib;oqs.lib;oqsprovider.lib;ws2_32.lib;
--------------------
servers
--------------------
jk-mtls-server        jk-mtls-server.exe      _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\servers	                                            ws2_32.lib;
                                                                          ..\..\..\src\common         
                                        ..\..\..\_deps\liboqs\include                     ..\..\..\_deps\liboqs\lib                         oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\servers;..\..\..\src\common;..\..\..\_deps\liboqs\include;..\..\..\_deps\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);
libssl.lib;libcrypto.lib;oqs.lib;oqsprovider.lib;ws2_32.lib;
----------
jk-https-server       jk-https-server.exe     _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\servers	                                            ws2_32.lib;
                                              CPPHTTPLIB_OPENSSL_SUPPORT  ..\..\..\src\common         
                                        ..\..\..\_deps\liboqs\include                     ..\..\..\_deps\liboqs\lib                         oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib
                                        ..\..\..\_deps\cpp-httplib\include                ..\..\..\_deps\cpp-httplib\lib\$(Configuration)   cpp-httplib-0.40.0.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE;CPPHTTPLIB_OPENSSL_SUPPORT
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\servers;..\..\..\src\common;..\..\..\_deps\cpp-httplib\include;..\..\..\_deps\liboqs\include;..\..\..\_deps\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\cpp-httplib\lib\$(Configuration);..\..\..\_deps\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);
libssl.lib;libcrypto.lib;cpp-httplib-0.40.0.lib;oqs.lib;oqsprovider.lib;ws2_32.lib;
----------
jk-https-dashboard    jk-https-dashboard.exe  _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\servers	                                            ws2_32.lib;
                                              CPPHTTPLIB_OPENSSL_SUPPORT  ..\..\..\src\common         
                                        ..\..\..\_deps\liboqs\include                     ..\..\..\_deps\liboqs\lib                         oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib
                                        ..\..\..\_deps\cpp-httplib\include                ..\..\..\_deps\cpp-httplib\lib\$(Configuration)   cpp-httplib-0.40.0.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE;CPPHTTPLIB_OPENSSL_SUPPORT
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\servers;..\..\..\src\common;..\..\..\_deps\cpp-httplib\include;..\..\..\_deps\liboqs\include;..\..\..\_deps\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\cpp-httplib\lib\$(Configuration);..\..\..\_deps\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);
libssl.lib;libcrypto.lib;cpp-httplib-0.40.0.lib;oqs.lib;oqsprovider.lib;ws2_32.lib;
----------
jk-pqc-server         jk-pqc-server.exe       _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\servers	                                            ws2_32.lib;
                                              CPPHTTPLIB_OPENSSL_SUPPORT  ..\..\..\src\common
                                        ..\..\..\_deps\$(Configuration)\liboqs\include    ..\..\..\_deps\$(Configuration)\liboqs\lib        oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib
                                        ..\..\..\_deps\libllhttp\include                  ..\..\..\_deps\libllhttp\lib                      llhttp.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE;CPPHTTPLIB_OPENSSL_SUPPORT
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\servers;..\..\..\src\common;..\..\..\_deps\$(Configuration)\liboqs\include;..\..\..\_deps\oqs-provider\include;..\..\..\_deps\libllhttp\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\$(Configuration)\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);..\..\..\_deps\libllhttp\lib;
libssl.lib;libcrypto.lib;oqs.lib;oqsprovider.lib;llhttp.lib;ws2_32.lib;
--------------------
tests
--------------------
test_provider         test_provider.exe       _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE
                                        ..\..\..\_deps\liboqs\include                     ..\..\..\_deps\liboqs\lib                         oqs.lib
                                        ..\..\..\_deps\oqs-provider\include               ..\..\..\_deps\oqs-provider\lib\$(Configuration)  oqsprovider.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE
$(OPENSSL_ROOT_DIR)\include;..\..\..\_deps\liboqs\include;..\..\..\_deps\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\_deps\liboqs\lib;..\..\..\_deps\oqs-provider\lib\$(Configuration);
libssl.lib;libcrypto.lib;oqs.lib;oqsprovider.lib;
----------
test_socketpool       test_socketpool.exe     _CRT_SECURE_NO_WARNINGS     ..\..\..\src\common                                               ws2_32.lib;                     ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE
..\..\..\src\common
ws2_32.lib;
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
```
