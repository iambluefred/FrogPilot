![](https://i.imgur.com/wpNJoJ6.png)

Table of Contents
=======================

* [What is openpilot?](#what-is-openpilot)
* [What is FrogPilot?](#what-is-frogpilot)
* [Features](#features)
* [How to Install](#how-to-install)
* [Bug reports / Feature Requests](#bug-reports--feature-requests)
* [Credits](#credits)
* [Licensing](#licensing)

---

What is openpilot?
------

[openpilot](http://github.com/commaai/openpilot) is an open source driver assistance system. Currently, openpilot performs the functions of Adaptive Cruise Control (ACC), Automated Lane Centering (ALC), Forward Collision Warning (FCW), and Lane Departure Warning (LDW) for a growing variety of [supported car makes, models, and model years](docs/CARS.md). In addition, while openpilot is engaged, a camera-based Driver Monitoring (DM) feature alerts distracted and asleep drivers. See more about [the vehicle integration](docs/INTEGRATION.md) and [limitations](docs/LIMITATIONS.md).

<table>
  <tr>
    <td><a href="https://youtu.be/NmBfgOanCyk" title="Video By Greer Viau"><img src="https://i.imgur.com/1w8c6d2.jpg"></a></td>
    <td><a href="https://youtu.be/VHKyqZ7t8Gw" title="Video By Logan LeGrand"><img src="https://i.imgur.com/LnBucik.jpg"></a></td>
    <td><a href="https://youtu.be/VxiR4iyBruo" title="Video By Charlie Kim"><img src="https://i.imgur.com/4Qoy48c.jpg"></a></td>
    <td><a href="https://youtu.be/-IkImTe1NYE" title="Video By Aragon"><img src="https://i.imgur.com/04VNzPf.jpg"></a></td>
  </tr>
  <tr>
    <td><a href="https://youtu.be/iIUICQkdwFQ" title="Video By Logan LeGrand"><img src="https://i.imgur.com/b1LHQTy.jpg"></a></td>
    <td><a href="https://youtu.be/XOsa0FsVIsg" title="Video By PinoyDrives"><img src="https://i.imgur.com/6FG0Bd8.jpg"></a></td>
    <td><a href="https://youtu.be/bCwcJ98R_Xw" title="Video By JS"><img src="https://i.imgur.com/zO18CbW.jpg"></a></td>
    <td><a href="https://youtu.be/BQ0tF3MTyyc" title="Video By Tsai-Fi"><img src="https://i.imgur.com/eZzelq3.jpg"></a></td>
  </tr>
</table>


What is FrogPilot? 🐸
------

FrogPilot is my custom "Frog Themed" fork of openpilot that has been tailored to improve the driving experience for my 2019 Lexus ES 350. I resync with the latest version of master quite frequently, so this fork is always up to date. I also strive to make every commit I make easy to read and easily cherry-pickable, so feel free to use any of my features in your own personal forks in any way that you see fit!

------

FrogPilot was last resynced to master on:

**May 14th, 2023**

Features
------

FrogPilot offers a wide range of customizable features that can be easily toggled on or off to suit your preferences. Whether you want a completely stock openpilot experience or want to add some fun and personal touches, FrogPilot has you covered! Some of the features include:

- Frog theme!
  - Frog/Green color scheme
  - Frog icons
  - Frog sounds (with a bonus Goat sound effect)
  - Frog turn signals that "hop" along the bottom of your screen
- Adjustable following distance profiles via the "Distance" button on the steering wheel (Toyota/Lexus only)
  - Other makes can use the Onroad UI button
- Allow the device to be offline indefinitely
- 'Back' button in the settings menu instead of the large 'X'
- Conditional Experimental Mode
  - Automatically enable "Experimental Mode" on curves
  - Automatically enable "Experimental Mode" when a turn signal is activated below 55mph
  - Automatically enable "Experimental Mode" when below a set speed
  - Automatically enable "Experimental Mode" when stop signs or stop lights are detected
- Custom steering wheel icons. Want to add your own? Message me on Discord at "FrogsGoMoo" #6969
- Customize the road UI
  - Increase or decrease the lane line width
  - Increase or decrease the path width
  - Increase or decrease the road edges width
  - Path edges that represent driving statuses (Experimental Mode on, Conditional Overridden, etc.)
  - "Unlimited" road UI that extends out as far as the model can see
- Device shuts down after being offroad for a set amount of time instead of 30 hours to help prevent battery drain
- Disable the wide camera while in Experimental Mode. This effect is purely cosmetic
- Easy Panda flashing via a "Flash Panda" button located within the "Device" menu
- Have the sidebar show by default to monitor your device temperature and connectivity with ease
- Nudgeless lane changes
  - Lane detection to prevent lane changes into curbs or going off-road
  - Optional one lane change per signal activation
- Numerical temperature gauge to replace the "GOOD", "OK", and "HIGH" temperature statuses
- On screen compass that rotates according to the direction you're facing
- Personal tune focused around my 2019 Lexus ES (TSS 2.0) which drives a bit more aggressively
- Prebuilt functionality for a faster boot
- Set the screen brightness to your liking (or even completely off while onroad)
- Steering wheel in the top right corner of the onroad UI rotates alongside your car's steering wheel
- Toggle Experimental Mode via the "Lane Departure Alert" button on your steering wheel (Toyota/Lexus only)
  - Other makes can simply double tap the screen while onroad
- ZSS (Zorro Steering Sensor) Support - *Currently in testing*

How to Install
------

Easiest way to install FrogPilot is via this URL at the installation screen:

```
https://installer.comma.ai/FrogAi/FrogPilot
```
Be sure to capitalize the "F" and "P" in "FrogPilot" otherwise the installation will fail.

DO NOT install the "FrogPilot-Development" branch. I'm constantly breaking things on there so unless you don't want to use openpilot, NEVER install it!

![](https://i.imgur.com/wxKp3JI.png)

Bug reports / Feature Requests
------

If you encounter any issues or bugs while using FrogPilot, or if you have any suggestions for new features or improvements, please don't hesitate to reach out to me. I'm always looking for ways to improve the fork and provide a better experience for everyone!

To report a bug or request a new feature, you can send me a message on Discord at "FrogsGoMoo #6969". Please provide as much detail as possible about the issue you're experiencing or the feature you'd like to see added. Screenshots, log files, and other relevant information are also helpful.

I will do my best to respond to bug reports and feature requests in a timely manner, but please understand that I may not be able to address every request immediately. Your feedback and suggestions are valuable, and I appreciate your help in making FrogPilot the best it can be!

As for feature requests, these are my standards:

- Can I test it on my 2019 Lexus ES or if I can't, are you up for testing it?
- Does it not require any panda code changes?
- How maintainable is it? Or will it frequently break with future openpilot updates?
- Is it not currently being developed by comma themselves? (i.e. Navigation)
- Will it actually be used or is it very niche?

Credits
------

* [Aragon7777](https://github.com/Aragon7777/openpilot)
* [DragonPilot](https://github.com/dragonpilot-community/dragonpilot)
* [KRKeegan](https://github.com/krkeegan/openpilot)
* [Move-Fast](https://github.com/move-fast/openpilot)
* [Pfeiferj](https://github.com/pfeiferj/openpilot)
* [Sunnyhaibin](https://github.com/sunnyhaibin/sunnypilot)
* [Twilsonco](https://github.com/twilsonco/openpilot)

Licensing
------

openpilot is released under the MIT license. Some parts of the software are released under other licenses as specified.

Any user of this software shall indemnify and hold harmless Comma.ai, Inc. and its directors, officers, employees, agents, stockholders, affiliates, subcontractors and customers from and against all allegations, claims, actions, suits, demands, damages, liabilities, obligations, losses, settlements, judgments, costs and expenses (including without limitation attorneys’ fees and costs) which arise out of, relate to or result from any use of this software by user.

**THIS IS ALPHA QUALITY SOFTWARE FOR RESEARCH PURPOSES ONLY. THIS IS NOT A PRODUCT.
YOU ARE RESPONSIBLE FOR COMPLYING WITH LOCAL LAWS AND REGULATIONS.
NO WARRANTY EXPRESSED OR IMPLIED.**

---

<img src="https://d1qb2nb5cznatu.cloudfront.net/startups/i/1061157-bc7e9bf3b246ece7322e6ffe653f6af8-medium_jpg.jpg?buster=1458363130" width="75"></img> <img src="https://cdn-images-1.medium.com/max/1600/1*C87EjxGeMPrkTuVRVWVg4w.png" width="225"></img>

[![openpilot tests](https://github.com/commaai/openpilot/workflows/openpilot%20tests/badge.svg?event=push)](https://github.com/commaai/openpilot/actions)
[![codecov](https://codecov.io/gh/commaai/openpilot/branch/master/graph/badge.svg)](https://codecov.io/gh/commaai/openpilot)
