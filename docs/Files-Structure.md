# 폴더/파일 구조 분석

[본문으로 이동](../README.md#전체-폴더파일-구성)

## 폴더/파일 구성

### 자동
```bash
# python 가상환경
$ conda env list
$ conda activate base
$ python -V

# JK_PyTools 경로에서, 구조 분석 수행
$ cd "D:\E\Study\GitHub-Local\repos\AI\Python\JK_PyTools"
$ python PyFolderViewer/pyfolderviewer.py "D:\E\Study\AI\AutoAgents\projects\JK-PQC" --hide-size --deny-dir=.git,.vs,bin,pre-certs --deny-ext=user --max-depth=10
```
```
D:\E\Study\AI\AutoAgents\projects\JK-PQC/
├── .blog/
│   ├── JK-PQC-18080.png
│   └── README.md # 블로그 게시물 작성
├── _deps/
│   ├── Debug/
│   │   ├── libllhttp/
│   │   │   ├── include/
│   │   │   │   └── llhttp.h
│   │   │   └── lib/
│   │   │       ├── cmake/
│   │   │       │   └── llhttp/
│   │   │       │       ├── llhttp-targets-debug.cmake
│   │   │       │       ├── llhttp-targets.cmake
│   │   │       │       ├── llhttpConfig.cmake
│   │   │       │       └── llhttpConfigVersion.cmake
│   │   │       ├── llhttp.lib
│   │   │       └── pkgconfig/
│   │   │           └── libllhttp.pc
│   │   └── liboqs/
│   │       ├── include/
│   │       │   └── oqs/
│   │       │       ├── aes_ops.h
│   │       │       ├── common.h
│   │       │       ├── kem.h
│   │       │       ├── kem_classic_mceliece.h
│   │       │       ├── kem_frodokem.h
│   │       │       ├── kem_kyber.h
│   │       │       ├── kem_ml_kem.h
│   │       │       ├── kem_ntruprime.h
│   │       │       ├── oqs.h
│   │       │       ├── oqsconfig.h
│   │       │       ├── rand.h
│   │       │       ├── sha2_ops.h
│   │       │       ├── sha3_ops.h
│   │       │       ├── sha3x4_ops.h
│   │       │       ├── sig.h
│   │       │       ├── sig_cross.h
│   │       │       ├── sig_dilithium.h
│   │       │       ├── sig_falcon.h
│   │       │       ├── sig_mayo.h
│   │       │       ├── sig_ml_dsa.h
│   │       │       ├── sig_sphincs.h
│   │       │       ├── sig_stfl.h
│   │       │       └── sig_uov.h
│   │       └── lib/
│   │           ├── cmake/
│   │           │   └── liboqs/
│   │           │       ├── liboqsConfig.cmake
│   │           │       ├── liboqsConfigVersion.cmake
│   │           │       ├── liboqsTargets-debug.cmake
│   │           │       └── liboqsTargets.cmake
│   │           ├── oqs.lib
│   │           └── pkgconfig/
│   │               └── liboqs.pc
│   ├── cpp-httplib/
│   │   ├── include/
│   │   │   └── httplib.h
│   │   └── lib/
│   │       ├── Debug/
│   │       │   ├── cpp-httplib-0.40.0.lib
│   │       │   └── cpp-httplib-0.40.0.pdb
│   │       └── Release/
│   │           └── cpp-httplib-0.40.0.lib
│   ├── libllhttp/
│   │   ├── include/
│   │   │   └── llhttp.h
│   │   └── lib/
│   │       ├── cmake/
│   │       │   └── llhttp/
│   │       │       ├── llhttp-targets-release.cmake
│   │       │       ├── llhttp-targets.cmake
│   │       │       ├── llhttpConfig.cmake
│   │       │       └── llhttpConfigVersion.cmake
│   │       ├── llhttp.lib
│   │       └── pkgconfig/
│   │           └── libllhttp.pc
│   ├── liboqs/
│   │   ├── include/
│   │   │   └── oqs/
│   │   │       ├── aes_ops.h
│   │   │       ├── common.h
│   │   │       ├── kem.h
│   │   │       ├── kem_classic_mceliece.h
│   │   │       ├── kem_frodokem.h
│   │   │       ├── kem_kyber.h
│   │   │       ├── kem_ml_kem.h
│   │   │       ├── kem_ntruprime.h
│   │   │       ├── oqs.h
│   │   │       ├── oqsconfig.h
│   │   │       ├── rand.h
│   │   │       ├── sha2_ops.h
│   │   │       ├── sha3_ops.h
│   │   │       ├── sha3x4_ops.h
│   │   │       ├── sig.h
│   │   │       ├── sig_cross.h
│   │   │       ├── sig_dilithium.h
│   │   │       ├── sig_falcon.h
│   │   │       ├── sig_mayo.h
│   │   │       ├── sig_ml_dsa.h
│   │   │       ├── sig_sphincs.h
│   │   │       ├── sig_stfl.h
│   │   │       └── sig_uov.h
│   │   └── lib/
│   │       ├── cmake/
│   │       │   └── liboqs/
│   │       │       ├── liboqsConfig.cmake
│   │       │       ├── liboqsConfigVersion.cmake
│   │       │       ├── liboqsTargets-release.cmake
│   │       │       └── liboqsTargets.cmake
│   │       ├── oqs.lib
│   │       └── pkgconfig/
│   │           └── liboqs.pc
│   └── oqs-provider/
│       ├── include/
│       │   └── oqs-provider/
│       │       └── oqs_prov.h
│       └── lib/
│           ├── Debug/
│           │   ├── oqsprovider.dll
│           │   ├── oqsprovider.exp
│           │   ├── oqsprovider.lib
│           │   └── oqsprovider.pdb
│           └── Release/
│               ├── oqsprovider.dll
│               ├── oqsprovider.exp
│               └── oqsprovider.lib
├── build/
│   └── msvc2022/
│       ├── JK-PQC.sln
│       ├── msvc-settings.md # Microsoft Visual C++ 2022 설정
│       └── projects/
│           ├── jk-https-client.vcxproj
│           ├── jk-https-client.vcxproj.filters
│           ├── jk-https-dashboard.vcxproj
│           ├── jk-https-dashboard.vcxproj.filters
│           ├── jk-https-server.vcxproj
│           ├── jk-https-server.vcxproj.filters
│           ├── jk-mtls-client.vcxproj
│           ├── jk-mtls-client.vcxproj.filters
│           ├── jk-mtls-server.vcxproj
│           ├── jk-mtls-server.vcxproj.filters
│           ├── jk-pqc-server.filters
│           ├── jk-pqc-server.vcxproj
│           ├── test_provider.vcxproj
│           ├── test_provider.vcxproj.filters
│           ├── test_socketpool.vcxproj
│           └── test_socketpool.vcxproj.filters
├── docs/
│   ├── Build-Dependencies.md # 종속 오픈소스 빌드하기
│   ├── Cursor-Settings.md # Cursor 구축 및 활용
│   ├── Files-Structure.md # 폴더/파일 구조 분석
│   ├── Making-Certs.md # `이중 인증서` 체계 구축
│   ├── README.md # 이 폴더는 `소유자(JK)가 기록한 문서 파일들을 저장`한다.
│   ├── images/
│   │   ├── JK-PQC-18080-mTLS.png
│   │   ├── JK-PQC-18080.png
│   │   ├── JK-PQC-18081.png
│   │   └── License-Apache2.0-blue.svg
│   └── log-https-apps.md # `jk-https-*` 어플 로그
├── src/
│   ├── README.md # 이 폴더는 `전반적인 소스 코드 파일들을 저장`한다.
│   ├── clients/
│   │   ├── README.md # 이 폴더는 `클라이언트 관련 어플`의 소스코드 파일들을 저장한다.
│   │   ├── jk-https-client.cpp
│   │   └── jk-mtls-client.cpp
│   ├── common/
│   │   ├── README.md # 이 폴더는 `공통`의 소스코드 파일들을 저장한다.
│   │   ├── SocketPool.cpp
│   │   ├── SocketPool.h
│   │   ├── https.cpp
│   │   ├── https.h
│   │   ├── json.hpp
│   │   ├── pqc_utils.cpp
│   │   ├── pqc_utils.h
│   │   ├── shm_stats.cpp
│   │   └── shm_stats.h
│   ├── servers/
│   │   ├── README.md # 이 폴더는 `서버 관련 어플`의 소스코드 파일들을 저장한다.
│   │   ├── jk-https-dashboard.cpp
│   │   ├── jk-https-server.cpp
│   │   ├── jk-mtls-server.cpp
│   │   ├── jk-pqc-server.cpp
│   │   ├── shm_consumer.cpp
│   │   └── shm_consumer.h
│   ├── tests/
│   │   ├── README.md # 이 폴더는 `테스트 관련 어플`의 소스코드 파일들을 저장한다.
│   │   ├── test_provider.c
│   │   └── test_socketpool.cpp
│   └── tools/
│       └── README.md # 이 폴더는 `툴 관련 어플`의 소스코드 파일들을 저장한다.
├── .gitignore
├── LICENSE
├── README.md # JK's PQC(양자내성암호) 인증 시스템 구축 및 개발
├── clean-all.bat
├── clean-bin.bat
├── gen-certs.bat
├── gen-dash-certs.bat
└── val-certs.bat
```

[본문으로 이동](../README.md#전체-폴더파일-구성)
