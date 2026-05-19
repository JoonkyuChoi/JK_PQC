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

--------------------
[2] 정적 어플리케이션
--------------------
jk-mtls-server        jk-mtls-server.exe      mTLS 인증 서버
                                              > 
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
프로젝트별 VC++ 설정
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
project					      output							    pre-define									inc-path		                lib-path							                req-libs						            out-path								              mid-path
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Debug
----------------------------------------------------
servers
--------------------
jk-mtls-server        jk-mtls-server.exe      _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\servers	      
                                                                          ..\..\..\src\common         
                                                                          ..\..\..\include            ..\..\..\bin\msvc\$(Configuration)    cpp-httplib-0.40.0.lib
                                        ..\..\..\bin\msvc\$(Configuration)\liboqs\include             ..\..\..\bin\msvc\$(Configuration)\liboqs\lib       oqs.lib
                                        ..\..\..\bin\msvc\$(Configuration)\oqs-provider\include       ..\..\..\bin\msvc\$(Configuration)\oqs-provider\lib oqsprovider.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\servers;..\..\..\src\common;..\..\..\include;..\..\..\bin\msvc\$(Configuration)\liboqs\include;..\..\..\bin\msvc\$(Configuration)\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\bin\msvc\$(Configuration);..\..\..\bin\msvc\$(Configuration)\liboqs\lib;..\..\..\bin\msvc\$(Configuration)\oqs-provider\lib;
libssl.lib;libcrypto.lib;cpp-httplib-0.40.0.lib;oqs.lib;oqsprovider.lib;
----------
jk-https-server       jk-https-server.exe     _CRT_SECURE_NO_WARNINGS     $(OPENSSL_ROOT_DIR)\include $(OPENSSL_ROOT_DIR)\lib               libssl.lib;libcrypto.lib        ..\..\..\bin\msvc\$(Configuration)\   $(OutDir)\obj\$(ProjectName)\
                                              STRSAFE_NO_DEPRECATE        ..\..\..\src\servers	      
                                              CPPHTTPLIB_OPENSSL_SUPPORT  ..\..\..\src\common         
                                                                          ..\..\..\include            ..\..\..\bin\msvc\$(Configuration)    cpp-httplib-0.40.0.lib
                                        ..\..\..\bin\msvc\$(Configuration)\liboqs\include             ..\..\..\bin\msvc\$(Configuration)\liboqs\lib       oqs.lib
                                        ..\..\..\bin\msvc\$(Configuration)\oqs-provider\include       ..\..\..\bin\msvc\$(Configuration)\oqs-provider\lib oqsprovider.lib

_CRT_SECURE_NO_WARNINGS;STRSAFE_NO_DEPRECATE;CPPHTTPLIB_OPENSSL_SUPPORT
$(OPENSSL_ROOT_DIR)\include;..\..\..\src\servers;..\..\..\src\common;..\..\..\include;..\..\..\bin\msvc\$(Configuration)\liboqs\include;..\..\..\bin\msvc\$(Configuration)\oqs-provider\include;
$(OPENSSL_ROOT_DIR)\lib;..\..\..\bin\msvc\$(Configuration);..\..\..\bin\msvc\$(Configuration)\liboqs\lib;..\..\..\bin\msvc\$(Configuration)\oqs-provider\lib;
libssl.lib;libcrypto.lib;cpp-httplib-0.40.0.lib;oqs.lib;oqsprovider.lib;

----------------------------------------------------
Release
----------------------------------------------------
servers
--------------------

------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
```
