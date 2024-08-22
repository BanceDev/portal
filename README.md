<p align="center">
  <img src="https://github.com/BanceDev/portal/blob/main/lobo.png">
</p>

---

# Portal

Portal is an open source group chat application designed with developers and creatives in mind. With an intuitive Lua scripting API each user is able to fully customize their client to their own personal preferences. Portal itself acts more as a custom protocol built using [Berkeley Sockets](https://en.wikipedia.org/wiki/Berkeley_sockets) where the base client is as minimal as possible to offer a clean slate to add on to.

## Compiling

First install [premake5](https://premake.github.io/download)

Then clone the repo and build, gmake uses gnu make but premake also supports making visual studio projects if you prefer that. Check out the [premake docs](https://premake.github.io/docs/) for more information.

```
git clone https://github.com/BanceDev/portal.git
cd portal
premake5 gmake
```

## Releases

Binary builds are available from [releases](https://github.com/BanceDev/portal/releases).

## Contributing

- For bug reports and feature suggestions please use [issues](https://github.com/BanceDev/portal/issues).
- If you wish to contribute code of your own please submit a [pull request](https://github.com/BanceDev/portal/pulls).
- Note: It is likely best to submit an issue before a PR to see if the feature is wanted before spending time making a commit.
- All help is welcome!
