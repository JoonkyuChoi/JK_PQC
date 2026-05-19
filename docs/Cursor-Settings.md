## Cursor 구축 및 활용

Cursor 설치 후, 회원 가입부터 사용법까지 `개발환경 설정`을 정리한 문서입니다.

[본문으로 이동](../README.md#cursor-구축-및-활용)  

## 개발환경 구성

### `Cursor` 참조
- [제품설명](https://cursor.com/ko/product)
- [대시보드](https://cursor.com/en-US/dashboard)
- [에이전트](https://cursor.com/agents)

### `Google` 계정으로, `Cursor` 가입/로그인
- Cursor는 회원가입 시, 고성능 모델(GPT-4o, Claude 3.5/3.7 등) 사용권 50회, 자동완성 2000회를 무료로 줍니다.  
- Cursor의 AI는 사용자의 PC가 아닌, 원격 서버(미국 본사)에서 돌아갑니다.  
  따라서, 서버와 통신하기 위한 인증 절차가 필수입니다.  
- 여러 기기를 사용할 경우(예: 데스크탑과 노트북), 설정이나 인덱싱 정보를 공유하기 위해, 계정을 사용합니다.
- 추가 설정
  * Privacy Mode (개인정보 보호): `Settings` > `General` > `Privacy Mode`를 **On**으로 설정

### Cursor 충돌 방지
- `코드 자동완성` 기능 끄기
  `GitHub Copilot`의 자동완성 기능은 월10$로 저렴합니다.
  `GitHub Copilot`을 사용하면, Cursor의 자동완성(`Cursor Tab`) 기능이 충돌하여, 코드가 겹쳐 보일 수 있습니다.
  `Cursor Tab`은 유료라서, 자동완성 사용횟수(2000회)를 초과하면 과금이 필요합니다.
  * 설정창(`Ctrl + Shift + J`) 열기
  * 좌측 `Tab` 항목 선택
  * 우측 `Cursor Tab` 비활성 선택

### 확장 플러그인 설치 (Ctrl+Shift+X)
- `GitHub Copilot` 설치하고, `Google` 계정으로 `Copilot` 로그인
  최근 Cursor 정책에 따라, `Copilot`을 터미널에서 제외시켰습니다.
  * [x] `GitHub Copilot`
  * [x] `Dev Spaces Copilot Chat Integration`
  * [x] `ChatGPT Copilot`
  * [o] [vsix 다운로드](https://marketplace.visualstudio.com/_apis/public/gallery/publishers/GitHub/vsextensions/copilot/latest/vspackage)
    * 속성창(Ctrl + Shift + P) 열기
    * `Extensions: Install from VSIX...` 선택
    * 다운로드한 `GitHub.copilot-latest.vsix` 선택하여 설치

### `GitHub Copilot` 사용
- 우측 하단에 `Copilot Completions` 상태가 `Open Memu` 여야 한다.
  좌측 하단에 출력된 8개의 Code를 [장치검토](https://github.com/login/device/confirmation) 사이트에 입력하면, Cursor 편집기와 연동된다.
- Python이나 C++ 소스코드 파일에서, 코드를 입력하다 보면 `추천 코드`가 나타난다.

### 기능 요약
- `코드 자동완성`은 `GitHub Copilot`으로 대체하여, `Cursor Tab`의 과금을 아끼자.
- `복잡한설계/리팩토링`은 Cursor의 `Composer(에이전트)` 기능을 사용하자. (월50회 무료)

### 한글 답변 지침
1.  설정창(`Ctrl + Shift + J`) 열기
2.  `General > Rules, Skills, Subagents` 섹션 선택
3.  `All, User, Project` 단위에서, `Project > Rules > +New` 클릭
4.  규칙 파일명: korean
5.  규칙 내용:  
    ```
    모든 답변은 한국어로 작성해.
    코드 주석도 한국어로 작성해.
    ```
6. 우측에 `Done` 버튼 클릭
7. `.cursor/rules/` 파일 생성 확인

### 기타 지침
`Project` 단위에서, `CLAUDE.md, AGENTS.md`는 기본적으로 포함되는 것처럼 되어 있지만, 제대로 동작하지 않는 것 같다.

### `Cursor` 단축키
- `Ctrl + I`  : 우측 AI관리창 열기/닫기
- `Ctrl + ~`  : 하단 터미널창 열기/닫기
- `Ctrl + K`  : 선택영역편집(Enter)/빠른질의(Alt+Enter)/AI채팅송신(Ctrl+L)

---

## 주요 정보

### AI 모드에 대하여
우측 `AI 관리창`은 아래의 4가지 모드로 동작합니다.
- Ask: 코드 수정없는 질의
- Plan: 작업 수행을 위한 세부 계획을 작성
- Agent: 무엇이든 계획하고, 검색하고, 구축하는 에이전트
- Debug: 시스템 진단과 실시간 추적을 이용한 버그 해결

### 과금 정책
```markdown
- 최신 Cursor 정책은 예전처럼 `채팅 N회 / Composer N회` 고정 횟수만으로 보기보다, **Usage pool(사용량 풀) 기반** 안내가 중심입니다.
  - `Auto + Composer` 사용량
  - `API` 사용량(플랜 포함 크레딧 내 차감)

- 현재 카운팅 상태(내 계정에서 지금 얼마나 썼는지)는 이 채팅에서 제가 직접 읽을 수 없고, 아래에서 확인해야 정확합니다.
  - [https://cursor.com/dashboard](https://cursor.com/dashboard)
  - [https://cursor.com/dashboard/usage](https://cursor.com/dashboard/usage)
```

[본문으로 이동](../README.md#cursor-구축-및-활용)  
