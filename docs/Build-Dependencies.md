# 종속 오픈소스 빌드하기

본 프로젝트를 구축하려면, 여러 라이브러리의 `소스 코드`를 빌드해야만 합니다.  
특히, `Hybrid (mTLS + PQC)` 상황에 맞추려면, 현재의 표준에 근접한 버전을 선택해야만 합니다.

> 중요:  
  현재(2026.05) 시점에, `Hybrid 인증서`는 비표준 상태입니다.  
  비표준으로 인하여 `Hybrid 인증서` 구축은 제외되었으며, 대신에 `이중 인증서` 개념을 적용하여, 브라우저의 인증도 수용할 수 있도록 하였습니다.  
  `PQC를 지원하지 않는 고전 브라우저`의 경우, `TLS-Handshake`단계에서 PQC 지원여부를 판단하여, `고전 ECDSA 인증서`로 서명(신원확인)하도록 처리하였습니다.  
  본 프로젝트는 `PQC 인증서 + mTLS + Hybrid 키교환` 기능을 적용시켜, 현재의 표준 PQC 통신을 구현하였습니다.

---

## 목차
- [의존 라이브러리들 구조](#의존-라이브러리들-구조)
- [의존 라이브러리들 오픈소스 버전 선택](#의존-라이브러리들-오픈소스-버전-선택)
- [의존 라이브러리들 소스 빌드 절차](#의존-라이브러리들-소스-빌드-절차)

---

## 개발 이력
```
- [2026.05.25] 종속 라이브러리들을 포함한 모든 프로젝트를 `/MD` 모드로 재빌드
                `KONG, APISIX` 게이트웨이들에 PQC를 적용시키기 위해. `/MD` 모드로 빌드된 결과물 필요
- [2026.05.23] llhttp-9.4.1 빌드 추가
- [2026.04.21] 최초 빌드 (openssl-3.5.4, cpp-httplib-0.40.0, liboqs-0.13.0, oqs-provider-0.9.0)
```

---

## 의존 라이브러리들 구조
```markdown
- llhttp        = HTTP
- cpp-httplib   = HTTP + OpenSSL
- liboqs        = OpenSSL + PQC
- oqs-provider  = OpenSSL + liboqs
- TLS + PQC
  * Client     (cpp-httplib + OpenSSL + oqs-provider)
  * Old Server (cpp-httplib + OpenSSL + oqs-provider)
  * New Server (llhttp      + OpenSSL + oqs-provider)
```

## 의존 라이브러리들 오픈소스 버전 선택
`Hybrid (mTLS + PQC)` 구축을 위해, 현재 상황에 적합한 아래의 3번째 버전들로 빌드하였습니다.

NIST OID 내장등록 생략하고, OpenSSL에 위임된 것을 사용한다:
- 탈락 (기능 미흡)
  * [libpki (C, Custom)](https://github.com/openca/libpki)
    - 기존 RSA/ECC가 구현된 라이브러리
    - PQC 지원 옵션을 켜고, `libpki-pqc`와 연동시켜 사용 (libpki 메인)
  * [libpki-pqc (C, MIT)](https://github.com/opencrypto/libpki-pqc)
    - 기존 libpki의 PQC 확장판으로 단독 사용 불가 (libpki 서브 역할)
    - Hybrid 인증서(RSA/ECC + PQC) 생성이 매우 용이
    - PQC 알고리즘의 표준 구현체인 `liboqs`를 백엔드로 사용하여, 알고리즘 제공
    - `X.509 인증서 구조, CMS, OCSP, CRL` 등 실제 PKI 운영에 필요한 고수준 API로 추상화
  * [pqcrypto (Python, Apache2)](https://github.com/backbone-hq/pqcrypto)
    - NIST PQC 표준화 공모전에서 검증된 주요 알고리즘들을 Python 환경에서 쉽게 사용할 수 있도록 패키징한 Python 바인딩 라이브러리  
  
- 탈락 (Hybrid 제외, 현재 버전은 PQC 전용)
  * [openssl (C/Ass/C++/Cuda, MIT)](https://github.com/openssl/openssl)
    - 현재(2026-04-22) 버전은 `4.0.0`
    - Composite Signature(Hybrid Cert 이중 서명) 구현에, `oqs-provider 0.9.x + liboqs 0.13.0` 연동시 충돌 위험
  * [liboqs (C/Ass/C++/Cuda, MIT)](https://github.com/open-quantum-safe/liboqs)
    - 현재(2026-04-22) 버전은 `0.15.0`
    - Composite Signature(Hybrid Cert 이중 서명)가 0.14.0에서 제거됨
  * [oqs-provider (C/C++, MIT)](https://github.com/open-quantum-safe/oqs-provider)
    - 현재(2026-04-22) 버전은 `0.11.0`
    - Composite Signature(Hybrid Cert 이중 서명)가 0.10.0-RC1에서 제거됨
  * [cpp-httplib (C++, MIT)](https://github.com/yhirose/cpp-httplib)
    - 현재(2026-04-22) 버전은 `0.43.1`
    - Docker 버전으로, 본 프로젝트에 부적합

- 선택 (Hybrid 병합)
  * [openssl (C/Ass/C++, MIT)](https://github.com/openssl/openssl/tree/openssl-3.5.4)
    - General purpose TLS and crypto library
      > git clone --branch openssl-3.5.4 --depth 1 https://github.com/openssl/openssl.git
    - 본 버전은 `3.5.4`
    - PKI 체인/인증서 발급
  * llhttp (C, MIT)
    - Port of http_parser to llparse
    - 본 버전은 `9.4.1`로써, Node.js에 내장된 것을 사용
    - 고성능 순수 HTTP 로써, TLS 인증은 별도 구현 필요
  * [liboqs (C/Ass/C++/Cuda, MIT)](https://github.com/open-quantum-safe/liboqs/tree/0.13.0)
    - C library for prototyping and experimenting with quantum-resistant cryptography
    - 본 버전은 `0.13.0`
    - `ML-KEM, ML-DSA, SLH-DSA, Falcon, BIKE, HQC` 등의 PQC 알고리즘 포함
    - MS 핵심 암호화 라이브러리인 `SymCrypt`에도 추가됨
  * [oqs-provider (C/C++, MIT)](https://github.com/open-quantum-safe/oqs-provider/tree/0.9.0)
    - OpenSSL 3 provider containing post-quantum algorithms
    - 본 버전은 `0.9.0`
    - 차세대 양자 알고리즘을 포함하는 OpenSSL 3 공급자
  * cpp-httplib (C++, MIT)
    - A C++ header-only HTTP/HTTPS server and client library
    - 본 버전은 `0.40.0`으로, llama-server에 내장된 것을 사용
    - OpenSSL, MbedTLS, wolfSSL 등과 연동하면 HTTPS 가능
  
## 의존 라이브러리들 소스 빌드 절차
```
1.  openssl (3.5.4)
    - 사전 설치
      > Perl
        https://strawberryperl.com
      > NASM 설치
        https://www.nasm.us
    - 빌드 구성 : 현재 시스템에 맞는 "include\openssl\configuration.h" 파일과, Makefile들 생성
      # 현재 빌드 결과물 확인 (/MD 확인)
      > openssl version -a
      > where openssl
      # 빌드 시작
      > cd D:\E\Study\AI\AutoAgents\_extra
      > cd openssl-3.5.4
      # Release
        > perl Configure VC-WIN64A --prefix=D:\openssl\3.5.4 --openssldir=D:\openssl\3.5.4\ssl CFLAGS="/MP8"            # 64비트 Windows (AMD64) - /MD
        > perl Configure VC-WIN64A-HYBRIDCRT --prefix=D:\openssl\3.5.4 --openssldir=D:\openssl\3.5.4\ssl CFLAGS="/MP8"  # 64비트 Windows (AMD64) - Hybrid 런타임 (/MT + ucrt.dll만 동적)
        > perl Configure VC-WIN32 --prefix=D:\openssl\3.5.4 --openssldir=D:\openssl\3.5.4\ssl CFLAGS="/MP8"             # 32비트 Windows (x86) - /MD
        > perl Configure VC-WIN32-HYBRIDCRT --prefix=D:\openssl\3.5.4 --openssldir=D:\openssl\3.5.4\ssl CFLAGS="/MP8"   # 32비트 Windows (x86) - Hybrid 런타임 (/MT + ucrt.dll만 동적)
        > perl Configure VC-WIN64I --prefix=D:\openssl\3.5.4 --openssldir=D:\openssl\3.5.4\ssl CFLAGS="/MP8"            # 64비트 Windows (Intel IA64) - 2021년 마지막, Itanium 지원을 Windows Server 2008 R2에서 종료
        > perl Configure VC-CE --prefix=D:\openssl\3.5.4 --openssldir=D:\openssl\3.5.4\ssl CFLAGS="/MP8"                # Windows CE
      # Debug
        > perl Configure VC-WIN64A --prefix=D:\openssl\3.5.4 --openssldir=D:\openssl\3.5.4\ssl --debug CFLAGS="/MP8"
    - 빌드/테스트/설치 수행
      > nmake           # 컴파일/링크 수행
      > nmake test      # 테스트 빌드 + 실행 (한번만 권장)
      > nmake install   # 빌드 결과물 설치 (perl Configure...에 설정한 경로에 복사)
    - 결과물 삭제
      > nmake clean     # nmake 명령의 모든 결과물 제거 (재빌드 준비)
      > nmake distclean # nmake + Configure 산출물 제거 (재 Configure 준비)
      > nmake uninstall # nmake install 경로 제거
    - 환경 변수 등록
      > SETX PATH             "D:\openssl\3.5.4\bin;D:\openssl\3.5.4\lib\ossl-modules;%PATH%"
      > SETX OPENSSL_ROOT_DIR "D:\openssl\3.5.4"
      > SETX OPENSSL_CONF     "D:\openssl\3.5.4\ssl\openssl.cnf"
2.  cpp-httplib (0.40.0)
    - 이전 빌드 결과물 확인 (/MD 확인)
      > dumpbin /directives D:\E\Study\AI\AutoAgents\projects\JK-PQC\_deps\cpp-httplib\lib\Debug\cpp-httplib-0.40.0.lib | findstr /i "Runtime LIBC MSVC"
    - 빌드 구성
      > cd D:\E\Study\AI\AutoAgents\_extra
      > cd cpp-httplib-0.40.0
      > mkdir build && cd build
      > /MD or /MDd
        cmake .. -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        /MT or /MTd
        cmake .. -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>"
    - 빌드 결과 경로 변경
      1.	`cpphttplib_ssl.sln` 파일을 MSVC에서 열기
      2.	좌측 `cpp-httplib-0.40.0` 프로젝트를 선택한 후, 속성창(Alt+F7) 열기
      3. 	구성=Debug   선택하고, 출력경로 아래와 같이 변경
          D:\E\Study\AI\AutoAgents\projects\JK-PQC\_deps\cpp-httplib\lib\$(Configuration)\
      4.  구성=Release 선택하고, 출력경로 동일하게 변경
      5.  MSVC 저장 후 종료
    - 빌드 수행
      > msbuild ALL_BUILD.vcxproj /p:Configuration=Debug /m:8
        msbuild ALL_BUILD.vcxproj /p:Configuration=Release /m:8
    - 빌드 결과물 수동 설치
      > MKDIR "D:\E\Study\AI\AutoAgents\projects\JK-PQC\_deps\cpp-httplib\include\"
      > COPY /B /Y "..\httplib.h" "D:\E\Study\AI\AutoAgents\projects\JK-PQC\_deps\cpp-httplib\include\"
    - 빌드 결과물 삭제
      > RMDIR /S /Q .
3.  liboqs (0.13.0)
    - 이전 빌드 결과물 확인 (/MD 확인)
      > dumpbin /directives D:\E\Study\AI\AutoAgents\projects\JK-PQC\_deps\Debug\liboqs\lib\oqs.lib | findstr /i "RuntimeLibrary LIBC MSVC"
    - 빌드 구성
      > cd D:\E\Study\AI\AutoAgents\_extra
      > cd liboqs-0.13.0
      > mkdir build && cd build
      > /MD or /MDd
        cmake .. -DOQS_USE_OPENSSL=ON -DOQS_USE_AES_OPENSSL=ON -DOQS_USE_SHA2_OPENSSL=ON -DOQS_USE_SHA3_OPENSSL=ON -DBUILD_SHARED_LIBS=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" -DCMAKE_INSTALL_PREFIX="D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/liboqs"
        
        /MT or /MTd
        cmake .. -DOQS_USE_OPENSSL=ON -DOQS_USE_AES_OPENSSL=ON -DOQS_USE_SHA2_OPENSSL=ON -DOQS_USE_SHA3_OPENSSL=ON -DBUILD_SHARED_LIBS=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>" -DCMAKE_INSTALL_PREFIX="D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/liboqs"
    - 빌드 수행
      > msbuild ALL_BUILD.vcxproj /p:Configuration=Debug /m:8
        msbuild ALL_BUILD.vcxproj /p:Configuration=Release /m:8
    - 빌드 설치
      > msbuild INSTALL.vcxproj /p:Configuration=Debug
        msbuild INSTALL.vcxproj /p:Configuration=Release
    - 설치 결과물 코드 확인/변경
      > "JK-PQC/_deps/liboqs/include/oqs/..."
      > "JK-PQC/_deps/liboqs/lib/pkgconfig/liboqs.pc" 파일을 열고, 경로가 올바른지 확인 : pc파일은 pkg-config 툴이 로드하는 라이브러리 메타데이터 파일입니다.
          prefix=D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/liboqs
    - Debug 모드는 복제 후 경로 변경
      > XCOPY /S /Y "D:\E\Study\AI\AutoAgents\projects\JK-PQC\_deps\liboqs" "D:\E\Study\AI\AutoAgents\projects\JK-PQC\_deps\Debug\liboqs"
      > "JK-PQC/_deps/liboqs/lib/pkgconfig/liboqs.pc" 파일을 열고, 경로 변경
        prefix=D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/Debug/liboqs
    - 빌드 결과물 삭제
      > RMDIR /S /Q .
    - 환경 변수 등록 (Release 버전 필수)
      > SETX liboqs_ROOT      "D:\E\Study\AI\AutoAgents\projects\JK-PQC\_deps\liboqs"
      > SETX liboqs_DIR       "D:\E\Study\AI\AutoAgents\projects\JK-PQC\_deps\liboqs\lib\cmake\liboqs"
4.  oqs-provider (0.9.0)
    - 사전 체크
      > openssl -v
      > echo %OPENSSL_ROOT_DIR%
      > echo %liboqs_ROOT%
        dir %liboqs_ROOT%\lib\*.lib
      > echo %liboqs_DIR%
      > perl -v
      > where msbuild
    - 이전 빌드 결과물 확인 (/MD 확인)
      > dumpbin /directives D:\openssl\3.5.4\lib\ossl-modules\oqsprovider.dll | findstr /i "RuntimeLibrary LIBC MSVC"
    - 빌드 구성
      > cd D:\E\Study\AI\AutoAgents\_extra
      > cd oqs-provider-0.9.0
      > mkdir build && cd build
      > /MD or /MDd
        cmake .. -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" -DCMAKE_INSTALL_PREFIX="D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/oqs-provider"
        /MT or /MTd
        cmake .. -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>" -DCMAKE_INSTALL_PREFIX="D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/oqs-provider"
    - 빌드 수행
      > msbuild ALL_BUILD.vcxproj /p:Configuration=Debug /m:8
        msbuild ALL_BUILD.vcxproj /p:Configuration=Release /m:8
    - 빌드 설치
      > msbuild INSTALL.vcxproj /p:Configuration=Debug
        msbuild INSTALL.vcxproj /p:Configuration=Release
    - 설치 결과물 이진파일들 확인
      > oqsprovider.dll 파일은 OpenSSL 경로에 설치됩니다.
        D:/openssl/3.5.4/lib/ossl-modules/oqsprovider.dll
      > 개발에 필요한 파일들은 수동으로 복제해야 합니다.
        XCOPY /S /Y ".\lib" "D:\E\Study\AI\AutoAgents\projects\JK-PQC\_deps\oqs-provider\lib\"
    - 설치 결과물 헤더파일 확인
      > "JK-PQC/_deps/oqs-provider/include/oqs-provider/oqs_prov.h"
    - 빌드 결과물 삭제
      > RMDIR /S /Q .
    - OpenSSL에 Provider 등록
      1. 환경변수로 임시 등록
        > SETX OPENSSL_MODULES  "D:\openssl\3.5.4\lib\ossl-modules"
      2. "openssl.cnf" 파일에 영구 등록
        > notepad %OPENSSL_CONF%
        ```ini
        [openssl_init]
        providers = provider_sect

        [provider_sect]
        default = default_sect
        oqsprovider = oqsprovider_sect

        [default_sect]
        activate = 1

        [oqsprovider_sect]
        module = D:\openssl\3.5.4\lib\ossl-modules\oqsprovider.dll
        activate = 1
        ```
      3. 확인
        > openssl list -providers
          openssl list -providers -provider oqsprovider
      4. 진단
        > where oqsprovider.dll
        > dumpbin /dependents D:\openssl\3.5.4\lib\ossl-modules\oqsprovider.dll
        > where libcrypto-3-x64.dll
5.  llhttp (9.4.1)
    - 이전 빌드 결과물 확인 (/MD 확인)
      > dumpbin /directives D:\E\Study\AI\AutoAgents\projects\JK-PQC\_deps\Debug\libllhttp\lib\llhttp.lib | findstr /i "RuntimeLibrary LIBC MSVC"
    - 빌드 구성
      > "https://github.com/nodejs/llhttp/archive/refs/tags/release/v9.4.1.zip" 다운로드 및 압축 해제
      > cd D:\E\Study\AI\AutoAgents\_extra
      > cd llhttp-release-v9.4.1
      > mkdir build && cd build
      > /MD or /MDd
        cmake .. -DLLHTTP_BUILD_SHARED_LIBS=OFF -DLLHTTP_BUILD_STATIC_LIBS=ON -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" -DCMAKE_INSTALL_PREFIX="D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/libllhttp"
        /MT or /MTd
        cmake .. -DLLHTTP_BUILD_SHARED_LIBS=OFF -DLLHTTP_BUILD_STATIC_LIBS=ON -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>" -DCMAKE_INSTALL_PREFIX="D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/libllhttp"
    - 빌드 수행
      > msbuild ALL_BUILD.vcxproj /p:Configuration=Debug /m:8
        msbuild ALL_BUILD.vcxproj /p:Configuration=Release /m:8
    - 빌드 설치
      > msbuild INSTALL.vcxproj /p:Configuration=Debug
        msbuild INSTALL.vcxproj /p:Configuration=Release
    - 설치 결과물 코드 확인/변경
      > "JK-PQC/_deps/libllhttp/include/llhttp.h"
      > "JK-PQC/_deps/libllhttp/lib/pkgconfig/libllhttp.pc" 파일을 열고, 경로가 올바른지 확인 : pc파일은 pkg-config 툴이 로드하는 라이브러리 메타데이터 파일입니다.
          prefix=D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/libllhttp
          exec_prefix=D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/libllhttp/bin
          libdir=D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/libllhttp/lib
          includedir=D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/libllhttp/include
    - Debug 모드는 복제 후 경로 변경
      > XCOPY /S /Y "D:\E\Study\AI\AutoAgents\projects\JK-PQC\_deps\libllhttp" "D:\E\Study\AI\AutoAgents\projects\JK-PQC\_deps\Debug\libllhttp"
      > "JK-PQC/_deps/libllhttp/lib/pkgconfig/libllhttp.pc" 파일을 열고, 경로 변경
          prefix=D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/Debug/libllhttp
          exec_prefix=D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/Debug/libllhttp/bin
          libdir=D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/Debug/libllhttp/lib
          includedir=D:/E/Study/AI/AutoAgents/projects/JK-PQC/_deps/Debug/libllhttp/include
    - 빌드 결과물 삭제
      > RMDIR /S /Q .
```
