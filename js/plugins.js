"use strict";

(function (exports) {
  function rmdashr(path) {
    const f = FS.open(path);
    const paths = Object.keys(f.node.contents);
    FS.close(f);
    if (f.node.isFolder) {
      for (const childPath of paths) rmdashr(path + "/" + childPath);
      FS.rmdir(path);
    } else {
      FS.unlink(path);
    }
  }

  function showPlugins(container, plugins) {
    container.innerHTML = "";
    container.style.display = "flex";
    container.style.flexDirection = "column";
    container.style.height = "30%";
    container.style.backgroundColor = "grey";
    container.style.overflow = "scroll";
    plugins.forEach(
      ({ name, url, version, author, iconUrl, description }, i) => {
        // TODO figure out a way to show icons (proxy?)
        const row = document.createElement("div");
        row.style.display = "flex";
        row.style.alignItems = "center";
        row.style.flexBasis = 1;
        row.style.flexGrow = 1;
        row.style.flexShrink = 1;
        row.style.padding = "5px";
        row.innerHTML = `
      <input style="flex: 1;" type="checkbox" name="plugin-${i}"/>
      <p style="flex: 2;" class="plugin-title"></p>
      <a style="flex: 1;" class="plugin-link" target="_blank" href="">link</a>
      <p style="flex: 6;" class="plugin-description"></p>
      <p style="flex: 2;" class="plugin-author"></p>
      `;

        let toDelete = null;

        // add dynamic values using DOM apis to prevent XSS injection
        row.querySelector("input").value = url;
        row.querySelector(".plugin-title").textContent = name;
        row.querySelector(".plugin-link").href = url;
        row.querySelector(".plugin-description").textContent = description;
        row.querySelector(".plugin-author").textContent = author;

        const proxy = "https://temp-cors-proxy.herokuapp.com/";
        const githubZip = `${proxy}${url}`;
        container.appendChild(row);

        row
          .querySelector("input")
          .addEventListener("change", async function (e) {
            if (this.checked) {
              this.disabled = true; // disable until download complete

              const data = await new Promise(function (resolve, reject) {
                JSZipUtils.getBinaryContent(githubZip, function (err, data) {
                  if (err) reject(err);
                  else resolve(data);
                });
              });
              const jszip = await JSZip.loadAsync(data);
              for (const name of Object.keys(jszip.files)) {
                if (!toDelete) toDelete = name;
                const zipObj = jszip.files[name];
                if (zipObj.dir) {
                  const path = ("/plugins/" + name).slice(0, -1); // remove trailing slash
                  FS.mkdir(path);
                  continue;
                } else {
                  const ab = await zipObj.async("ArrayBuffer");
                  const stream = FS.open("/plugins/" + name, "w+");
                  FS.write(stream, new Uint8Array(ab), 0, ab.byteLength, 0);
                  FS.close(stream);
                }
              }
              this.disabled = false;
            } else {
              const path = `/plugins/${toDelete}`;
              rmdashr(path);
            }
          });
      }
    );
  }

  async function getAllFileEntries(dataTransferItemList) {
    let fileEntries = [];
    let queue = [];
    for (let i = 0; i < dataTransferItemList.length; i++) {
      queue.push(dataTransferItemList[i].webkitGetAsEntry());
    }
    while (queue.length > 0) {
      let entry = queue.shift();
      if (entry.isFile) {
        fileEntries.push(entry);
      } else if (entry.isDirectory) {
        queue.push(...(await readAllDirectoryEntries(entry.createReader())));
      }
    }
    return fileEntries;
  }

  async function readAllDirectoryEntries(directoryReader) {
    let entries = [];
    let readEntries = await readEntriesPromise(directoryReader);
    while (readEntries.length > 0) {
      entries.push(...readEntries);
      readEntries = await readEntriesPromise(directoryReader);
    }
    return entries;
  }

  async function readEntriesPromise(directoryReader) {
    try {
      return await new Promise((resolve, reject) => {
        directoryReader.readEntries(resolve, reject);
      });
    } catch (err) {
      console.log(err);
    }
  }

  function showPluginUpload(container) {
    container.innerHTML = "";
    container.style.display = "flex";
    container.style.flexDirection = "column";
    container.style.backgroundColor = "grey";
    container.style.overflow = "scroll";

    container.innerHTML = `
      <input style="align-self: center;" type="file" directory allowdirs webkitdirectory/>
      <div id="drop-target" style="flex: 1; position: inset;"></div>
      `;
    const input = container.querySelector("input");
    const dropTarget = container.querySelector("#drop-target");

    function parentDirsFromRoot(path) {
      const dirs = [];
      let parent = "";
      const parts = path.split("/");
      for (const part of parts.slice(0, parts.length - 1)) {
        if (!part) continue; // slash always already exists
        parent = parent + "/" + part;
        dirs.push(parent);
      }
      return dirs;
    }

    // intermediate representation so this works with drag and drop
    // (which uses fileentries) and input (which uses files)
    async function addPluginFiles(filesAndPaths) {
      for (const { file, path } of filesAndPaths) {
        // TODO I think these are always unix-like / paths? check Windows
        const dest = "/plugins" + (path[0] === "/" ? path : "/" + path);
        for (const dir of parentDirsFromRoot(dest)) {
          if (!FS.analyzePath(dir).exists) {
            FS.mkdir(dir);
          }
        }
        const ab = await file.arrayBuffer();
        const stream = FS.open(dest, "w+");
        FS.write(stream, new Uint8Array(ab), 0, ab.byteLength, 0);
        FS.close(stream);
        console.log("wrote data to", dest);
      }

      input.style.display = "none";
      container.innerHTML =
        "Uploaded " + filesAndPaths.length + " plugin files";
    }

    input.addEventListener("input", async (e) => {
      // TODO does this always contain all files?
      await addPluginFiles(
        [...e.target.files].map((f) => ({
          path: f.webkitRelativePath,
          file: f,
        }))
      );
    });

    function restoreTinyDropzone() {
      dropTarget.style.outline = "";
      dropTarget.style.zIndex = "";
      dropTarget.style.top = "";
      dropTarget.style.left = "";
      dropTarget.style.width = "";
      dropTarget.style.height = "";
      dropTarget.style.position = "";
      dropTarget.style.opacity = "";
      dropTarget.style.backgroundColor = "";
    }

    dropTarget.addEventListener(
      "drop",
      async function (event) {
        event.preventDefault();
        restoreTinyDropzone();
        container.querySelector("input").files = event.dataTransfer.files;
        const all = await getAllFileEntries(event.dataTransfer.items);
        addPluginFiles(
          await Promise.all(
            all.map(async (fileEntry) => {
              return {
                path: fileEntry.fullPath,
                file: await new Promise((r) => fileEntry.file(r)),
              };
            })
          )
        );
      },
      false
    );
    document.querySelector("html").addEventListener("dragenter", (e) => {
      dropTarget.style.outline = "solid 5px blue";
      dropTarget.style.zIndex = 100;
      dropTarget.style.top = "0";
      dropTarget.style.left = "0";
      dropTarget.style.width = "100vw";
      dropTarget.style.height = "100vh";
      dropTarget.style.position = "absolute";
      dropTarget.style.opacity = "0.5";
      dropTarget.style.backgroundColor = "grey";
      e.preventDefault();
    });
    dropTarget.addEventListener("dragenter", (e) => {
      e.preventDefault();
    });
    dropTarget.addEventListener("dragover", (e) => {
      e.preventDefault();
    });
    dropTarget.addEventListener("dragleave", (e) => {
      restoreTinyDropzone();
    });
  }

  window.showPlugins = showPlugins;
  window.showPluginUpload = showPluginUpload;
})();
