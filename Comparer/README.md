## TODO
- [ ] 영상 드래그시 움직임이 부드럽지 않다
- [ ] 아래 프레임 별 PSNR을 보여주는 창에서도 클릭으로 해당 프레임으로 이동되면 좋을 것 같다
- [ ] 커서의 Pixel 위치 보여주기 (Viewer에 있는 기능)
- [ ] 지금 ComparerView에서만 Space로 Play할 수 있다. PosInfoView에서도 Play할 수 있도록 수정하기
- [ ] FrmInfoView에 보여주는 계산을 Thread로 하기 (Loading 속도 개선)
- [ ] README.md에 FrmInfoView (중간 PSNR, SSIM 텍스트), FrmsInfoView(그래프), PosInfoView(왼쪽 빨간색라인)의 구조보여주기
- [ ] 픽셀단위로 틀린 부분 보여주기 (분홍색 overlay로 표시)
- [ ] 좌우 윈도우에 x(close) 버튼 만들기
- [ ] File Open Dialog에서 파일 두개 지정할 수 있게 하기
- [ ] 명령어 인자에 두개의 파일 지원하기
- [ ] 아래 FrmsInfoView에서 마우스 스크롤로 graph확대 축소하기
- [ ] 파일이 변경되었을 때 자동으로 update하게 하기
- [ ] Viewer에서 처럼 같은 해상도의 이미지가 같은 폴더에 있을 때 연속적으로 보여주도록 수정
- [ ] 이미지보는 배율을 바꾼 상태에서 새로운 이미지를 Drag&Drop으로 Open했을때 배율이 리셋되지 않아서 확대된 새 이미지가 보여지는 문제 (minor)

# DONE
- [x] zoom in-out할때 exp() ratio 사용
- [x] 축소하는 코드도 확대하는 코드와 같이 쓰도록
- [x] 확대시 회색의 픽셀 boundary (grid) 보여주기
- [x] 확대시 RGB value 보여주기
- [x] 'h' hotkey로 hex, decimal 변환하기
- [x] 비율보여주기
- [x] Video의 Frame도 보여주기
- [x] Viewer처럼 FPS를 조절 할 수 있도록 수정하기
- [x] 좌우 윈도우에 파일이름 보여주기
- [x] Interpolation 적용할 수 있도록 하기 (Viewer에 있는 기능)
- [x] PSNR 및 SSIM을 제대로 계산하고 있는지 확인 필요 -> OpenCV와 TensorFlow 방법으로 수정
- [x] 이미지 크기가 달라도 비교할 수 있도록 지원 (0a863c9d)
