# 블로그 게시물 작성

본 문서는 [개인 블로그 사이트](https://blog.jk-dreams.com/) 게시용 정보를 보관한 것이다.

![대표 이미지](JK-PQC-18080.png)

## Title
JK-PQC

## Excerpt
JK's PQC(양자내성암호) 인증 시스템 구축

## Tag
portfolio

## 개발 정보
```
- 개발 이력
  [2026.05.18 ~ 2026.05.19] 공개용 편집 및 GitHub 등록
  [2025.04.21 ~ 2026.05.16] Windows PQC mTLS 인증 시스템 구축
- 개발 환경 및 툴
  Cursor, MSVC2022, Perl, nasm, CMake
- 개발 언어
  > C/C++
- 종속 라이브러리들
  > openssl (3.5.4) : https://github.com/openssl/openssl/tree/openssl-3.5.4
  > liboqs (0.13.0) : https://github.com/open-quantum-safe/liboqs/tree/0.13.0
  > oqs-provider (0.9.0) : https://github.com/open-quantum-safe/oqs-provider/tree/0.9.0
  > cpp-httplib (0.40.0) : https://github.com/ggml-org/llama.cpp/tree/master/vendor/cpp-httplib
- 데이터 관리
  > File
- 통신
  > Socket
```

## 설명
본 게시물은 [GitHub](https://github.com/JoonkyuChoi/JK-PQC) 저장소에 공개하였습니다.

급변하는 인공지능 시대에, PQC(양자내성암호) 인증 시스템을 선제적으로 구축하여, 다가오는 양자컴퓨터 시대에 대응하고자 프로젝트를 진행하였습니다.

### 배경
현재 널리 사용되는 `RSA, ECDSA` 등 고전 암호화 알고리즘은 양자컴퓨터의 Shor 알고리즘에 의해, 수년 내 해독 가능해질 것으로 예측됩니다.  
NIST는 이에 대응하여, 2024년 ML-KEM(FIPS 203), ML-DSA(FIPS 204), SLH-DSA(FIPS 205)를 PQC 표준으로 공식 발표하였습니다.

### 주요 기능
- OpenSSL 3.5.x + oqs-provider 기반 PQC 인증서 발급 및 검증
- ML-KEM, ML-DSA 알고리즘을 활용한 Hybrid mTLS 통신
- 고전 암호(X25519, ECDSA)와 PQC를 결합한 Hybrid 방식으로, 전환기 호환성 확보
- cpp-httplib 기반 경량 HTTPS 서버/클라이언트 구현

### 개발 방식
- 기획 : 저작자 (JK), Claude.ai
- 설계/개발 지침 : Claude.ai, 저작자
- 코딩 : `Cursor Composer`, 저작자
- 번역 : Gemini, Groq, Cerebras, Gemma4 (local)  
  번역은 Python으로 개발한 CLI 툴을 사용하여, 온라인 AI모델 API 서버를 사용하거나, 일일 토큰량이 소모되면 로컬 Gemma4 모델을 사용하였습니다.  
  `Python CLI 번역툴`의 소스코드는 공개하지 않았습니다.  
  주로, 종속 라이브러리들의 오픈소스에 포함된 Markdown 파일들을 번역하여, 분석에 활용하였습니다.
