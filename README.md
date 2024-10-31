<header>

# **计算机网络实验报告**
## LAB2：配置Web服务器
## 2211321 申展

## 1.实验要求
（1）搭建Web服务器（自由选择系统），并制作简单的Web页面，包含简单文本信息（至少包含专业、学号、姓名）、自己的LOGO、自我介绍的音频信息。

（2）通过浏览器获取自己编写的Web页面，使用Wireshark捕获浏览器与Web服务器的交互过程，使用Wireshark过滤器使其只显示HTTP协议，，对HTTP交互过程进行详细说明。
## 2.Web页面功能展示、代码分析
### 2.1实验环境说明
使用Python的Web框架Flask，在 Flask 中设置一个基本的 Web 服务器，并编写 HTML 页面代码，以满足实验要求。
### 2.2功能展示
运行 Flask 服务器，打开浏览器访问(http://127.0.0.1:8080)进行测试，发现页面内容可以被正确加载如下。

### 2.3代码分析
Flask 服务器代码：
```python
from flask import Flask, render_template

app = Flask(__name__)

@app.route("/")
def home():
    return render_template("index.html")
if __name__ == "__main__":
    app.run(app.run(host='0.0.0.0', port=8080))
```
html页面：
```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <title>个人主页</title>
    <link rel="icon" href="{{ url_for('static', filename='favicon.ico') }}" type="image/x-icon">
    <style type="text/css">
        body {
            background-image: url("static/background.png");
            background-repeat: no-repeat;
            background-size: cover;
            background-attachment: fixed;
        }
        .maoboli {
            max-width: 350px;
            margin: 50px auto;
            padding: 20px;
            background: rgba(255, 255, 255, 0.5);
            backdrop-filter: blur(10px);
            border-radius: 15px;
            box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.2);
            text-align: center;
        }
        h1 {
            font-size: 24px;
            margin-top: 0;
        }
        img {
            max-width: 100%;
            height: auto;
            margin-top: 20px;
            border-radius: 5px;
        }
    </style>
</head>
<body>
    <div class="maoboli">
        <h1>欢迎来到我的主页</h1>
        <img src="{{ url_for('static', filename='profile.png') }}">
        <p>学号：2211321</p>
        <p>姓名：申展</p>
        <p>专业：密码科学与技术</p>
        <audio controls>
            <source src="{{ url_for('static', filename='Captain.mp3') }}" type="audio/mpeg">
        </audio>
    </div>
</body>
</html>
```
## 3.抓包结果与HTTP交互过程分析
### 3.1利用wireshack实现抓包
### 3.2HTTP交互过程分析



