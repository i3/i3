i3 - Unofficial Gaps Branch
===========================

This branch uses the gaps patch from:
http://infra.in.zekjur.net/pipermail/i3-discuss/2012-November/001042.html

I have also included some minor fixes that, for me at least, makes it work perfectly.

~~As well as this there is now a little bit more convenient way to change the gap size by
modifying the GAP_SIZE macros near the top of src/render.c~~.

Gap sizes are now set in the config file by adding:
```
gap_size 16
```



And the result:
(click the image below to see a video of it in action)

[![i3](http://devthe.com/files/i3.png "i3")](http://devthe.com/files/i3demo.webm)

