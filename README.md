# ESP32 Berry Demo
在ESP32运存[berry](https://github.com/berry-lang/berry)语言

## 示范
```
berry>  print("hello")
hello
berry>  def add(x, y)
berry>>     return x+y
berry>> end
berry>  add(3, 4)
7
berry>  fun = compile("/spiffs/json.be", "file")
berry>  fun()
{'key': 'value'}
{"test key":null}
{
  "45": true,
  "key1": null
}
berry>
```
<p align="left">
<img src="./screenshot/shot.gif" alt="Demonstration">
</p>
