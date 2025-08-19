# windows
1. 查看摄像头设备的名字
    * ffmpeg -list_devices true -f dshow -i dummy
2. 发流，设备名称要修改成自己的。使用TCP，使用UDP失败了，可能是公司防火墙设置问题。摄像头原始流格式可能需要修改
    * ffmpeg -f dshow -vcodec mjpeg -video_size 1280x720 -framerate 30 -i video="Integrated Webcam" -c:v libx264 -pix_fmt yuv420p -preset fast -f mpegts tcp://0.0.0.0:1234?listen

# wsl2 ubuntu
1. 测试是否可以正常取流，地址根据自己的ipconfig修改
    * ffplay tcp://192.168.56.1:1234

# build 
* bazel-bin/src/pullStreamAndSave/pullStreamAndSave