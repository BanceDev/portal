<p align="center">
  <img width="256" height="256" src="https://github.com/BanceDev/portal/blob/main/logo.png">
</p>

---

# Portal

Portal is an open source group chat application designed with developers and creatives in mind. With an intuitive Lua scripting API each user is able to fully customize their client to their own personal preferences. Portal itself acts more as a custom protocol built using [Berkeley Sockets](https://en.wikipedia.org/wiki/Berkeley_sockets) where the base client is as minimal as possible to offer a clean slate to add on to.

## Compiling

Clone the repo and run the install script.

```
git clone https://github.com/BanceDev/portal.git
cd portal
sh install.sh
```
NOTE: The install.sh script only supports apt, pacman, dnf, and yum package managers. If your package manager is not listed or you are trying to get a build running on windows here is a list of all the dependencies. ```glfw3, cglm, libcclipboard, xcb, sqlite3, glxinfo/glx-utils/mesa-utils```


For future compiles just run ```make``` from the root directory. If you change the premake5.lua file rebuild the makefiles with ```premake5 gmake```.

## Running the Server From Docker

If you want to run your own instance of the portal server using Docker its quite simple.

First just install [Docker](https://www.docker.com/) for your operating system.

Then run the following commands to build and run:

```
docker build -t portal-server .
docker run -it --rm portal-server
```

## Releases

Binary builds are available from [releases](https://github.com/BanceDev/portal/releases).

## Contributing

- For bug reports and feature suggestions please use [issues](https://github.com/BanceDev/portal/issues).
- If you wish to contribute code of your own please submit a [pull request](https://github.com/BanceDev/portal/pulls).
- Note: It is likely best to submit an issue before a PR to see if the feature is wanted before spending time making a commit.
- All help is welcome!
