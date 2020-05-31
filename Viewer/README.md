## Todo
- [ ] 한글 파일 열때 문제 없도록...
  -  궁극적으로 opencv의 findDecoder에서 _wfopen을 통해서 읽을 수 있어야 한다.
- [ ] Input Sync
  - Viewer가 여러 개 실행되고 있다면 특정한 키를 통해서 Input Event가 서로 공유될 수 있도록 하면 여러 이미지와 비교도 가능할 것이다 (Comparer는 두개 이미지만 비교 가능하다).
  - 구현방법을 살짝 기술하면, 제일 먼저 실행되는 Viewer를 IPC나 Socket통신의 서버로 동작하게 하고,
  이후 여러개의 Viewer가 실행되었을 때 하나의 Viewer process에서 발생되는 input event를 Observer 패턴을 통해서 Broadcast할 수 있을 것 같다.
  - 제일 먼저 샐행되는 Viewer process가 실행되는 Viewer process의 리스트를 가지고 있고 사용자의 input event를 broadcasting하는 process가 될 것이다.
- [ ] 정확한 FPS 구현
  과거 10초 동안 처리된 수와 처리해야 할 frame수를 비교해서 그 비율을 구하고 그것에 따라서 time간격을 다시 setting 하기
- [ ] Support audio play

## Done
- [x] mouse pointer의 좌표 정보 보여주기
- [x] CS 및 Size를 UI에서 수동으로도 설정 가능하도록 하기
- [x] 마우스 클릭시에 포인터 모양 손모양으로 바꾸기 (progessive bar에서는 예외)
- [x] 아래 진행바를 클릭하여 장면 이동하기
- [x] TIFF 지원
- [x] RGB888 dump 기능
- [x] PageDown, Up으로 다음 이미지 보여주기
- [x] 스크린 화면보다 이미지가 더 클때 스크린 사이즈만 보여주기
- [x] 마우스 위치를 중심으로 Zoom in & out 되기
- [x] Make it easy to resize bounding boxes
- [x] 화면 clipboard
- [x] select mode 해제 되어도 선택영역 그대로 보이게 하기
- [x] 다른 app에서 capture한 image paste하기
- [x] 확대할 때, smooth하게 확대하고(초반 배율은 낮게) 배율이 증가할 수록 더 exponential하게 배율이 증가하도록... (좀더 사람의 눈에 맞는 zoom in-out)
- [x] Y만 보여주기
- [x] comparer에도 exponential하게 확대하기
- [x] RGB 값은 많이 확대되면 각 pixel에 보여주기
- [x] 확대됐을때 grid도 보여주기
- [x] 'h' key를 누르면 color가 hexa로 보이게
- [x] 사각형으로 선택된 부분만 CTRL+C 로 clipboard에 copy하기
- [x] 전체화면 mode
- [x] When ? key is pressed, show all the shortcuts
- [x] 단축키 및 콘솔 환경에서 명령어 라인으로 동작 기능
- [x] YUV420, NV21 dump기능
- [x] Viewer rotation 기능
- [x] Home/End 키를 누르면 처음과 끝으로 가도록...
- [x] Viewer 좌표를 우측 아래에 특정 키를 누르면 보여주기 (제목에서는 제거)
- [x] 좋은 resampler (bicubic)를 사용해서 화면에 보여주기
- [x] Resize 기능
- [x] 파일의 변화 알아채서 다시 읽기
- [x] 선택된 box의 크기를 보여주기
- [x] 선택된 box가 움직일 수 있도록 하기

## Known issues
- Only even width are supported for YUV formats
- ASCII file names are only allowed