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
$ python PyFolderViewer/pyfolderviewer.py "D:\E\Study\GitHub-Local\repos\AI\Gateways\@JK-PQC" --hide-size --deny-dir=.git,.vs,bin --deny-ext=user --max-depth=10
```
```
D:\E\Study\GitHub-Local\repos\AI\Gateways\@JK-PQC/
├── .blog/
│   ├── JK-PQC-18080.png
│   └── README.md # 블로그 게시물 작성
├── .cursor/
│   └── rules/
│       ├── identity.mdc
│       └── korean.mdc
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
│           ├── test_provider.vcxproj
│           └── test_provider.vcxproj.filters
├── docs/
│   ├── Build-Dependencies.md # 종속 오픈소스 빌드하기
│   ├── Cursor-Settings.md # Cursor 구축 및 활용
│   ├── Files-Structure.md # 폴더/파일 구조 분석
│   ├── Making-Certs.md # `이중 인증서` 체게 구축
│   ├── README.md # 이 폴더는 `소유자(JK)가 기록한 문서 파일들을 저장`한다.
│   ├── images/
│   │   ├── JK-PQC-18080-mTLS.png
│   │   ├── JK-PQC-18080.png
│   │   ├── JK-PQC-18081.png
│   │   └── License-Apache2.0-blue.svg
│   └── log-https-apps.md # `jk-https-*` 어플 로그
├── include/
│   ├── httplib.h
│   └── json.hpp
├── src/
│   ├── README.md # 이 폴더는 `전반적인 소스 코드 파일들을 저장`한다.
│   ├── clients/
│   │   ├── README.md # 이 폴더는 `클라이언트 관련 어플`의 소스코드 파일들을 저장한다.
│   │   ├── jk-https-client.cpp
│   │   └── jk-mtls-client.cpp
│   ├── common/
│   │   ├── README.md # 이 폴더는 `공통`의 소스코드 파일들을 저장한다.
│   │   ├── https.cpp
│   │   ├── https.h
│   │   ├── pqc_utils.cpp
│   │   ├── pqc_utils.h
│   │   ├── shm_stats.cpp
│   │   └── shm_stats.h
│   ├── servers/
│   │   ├── README.md # 이 폴더는 `서버 관련 어플`의 소스코드 파일들을 저장한다.
│   │   ├── jk-https-dashboard.cpp
│   │   ├── jk-https-server.cpp
│   │   ├── jk-mtls-server.cpp
│   │   ├── shm_consumer.cpp
│   │   └── shm_consumer.h
│   ├── tests/
│   │   ├── README.md # 이 폴더는 `테스트 관련 어플`의 소스코드 파일들을 저장한다.
│   │   └── test_provider.c
│   └── tools/
│       └── README.md # 이 폴더는 `툴 관련 어플`의 소스코드 파일들을 저장한다.
├── .gitignore
├── LICENSE
├── README.md # JK's PQC(양자내성암호) 인증 시스템 구축
├── clean-all.bat
├── clean-bin.bat
├── gen-certs.bat
├── gen-dash-certs.bat
└── val-certs.bat
```

[본문으로 이동](../README.md#전체-폴더파일-구성)
