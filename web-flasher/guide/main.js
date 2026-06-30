import "../src/theme.css";
import "./styles.css";

import { guideContent, isExternalUrl, isYouTubeId } from "./content.js";

const lightbox = document.querySelector("#image-lightbox");
const lightboxImage = document.querySelector("#image-lightbox-image");
const lightboxCaption = document.querySelector("#image-lightbox-caption");
const lightboxClose = document.querySelector("#image-lightbox-close");

function openImageLightbox({ src, alt, title }) {
  lightboxImage.src = src;
  lightboxImage.alt = alt;
  lightboxCaption.textContent = title || alt;
  lightbox.showModal();
}

lightboxClose.addEventListener("click", () => lightbox.close());
lightbox.addEventListener("click", (event) => {
  if (event.target === lightbox) {
    lightbox.close();
  }
});
lightbox.addEventListener("close", () => {
  lightboxImage.removeAttribute("src");
  lightboxImage.alt = "";
  lightboxCaption.textContent = "";
});

function createExternalLink(action) {
  if (!action?.url || !isExternalUrl(action.url)) {
    const pending = document.createElement("span");
    pending.className = "pending-link";
    pending.textContent = action?.label || "Coming soon";
    return pending;
  }

  const anchor = document.createElement("a");
  anchor.className = "inline-link";
  anchor.href = action.url;
  anchor.target = "_blank";
  anchor.rel = "noopener noreferrer";
  anchor.textContent = action.label;
  return anchor;
}

function createMediaSlot({ file, title, alt }, shape = "wide") {
  const figure = document.createElement("figure");
  figure.className = `media-slot media-${shape}`;

  const frame = document.createElement("div");
  frame.className = "media-frame";

  const image = document.createElement("img");
  image.src = file;
  image.alt = alt;
  image.loading = "lazy";

  const zoomButton = document.createElement("button");
  zoomButton.className = "media-zoom-button";
  zoomButton.type = "button";
  zoomButton.disabled = true;
  zoomButton.setAttribute("aria-label", `Enlarge ${title || alt}`);
  zoomButton.addEventListener("click", () => {
    openImageLightbox({ src: file, alt, title });
  });

  const fallback = document.createElement("div");
  fallback.className = "media-fallback";
  const fallbackTitle = document.createElement("strong");
  fallbackTitle.textContent = "Screenshot placeholder";
  const fallbackFile = document.createElement("code");
  fallbackFile.textContent = file.split("/").pop();
  fallback.append(fallbackTitle, fallbackFile);

  image.addEventListener("load", () => {
    zoomButton.disabled = false;
    frame.classList.add("has-image");
  });
  image.addEventListener("error", () => {
    zoomButton.disabled = true;
    frame.classList.remove("has-image");
  });

  frame.append(image, zoomButton, fallback);
  figure.append(frame);
  if (title) {
    const caption = document.createElement("figcaption");
    caption.textContent = title;
    figure.append(caption);
  }
  return figure;
}

function renderVideo() {
  const container = document.querySelector("#video-container");
  if (isYouTubeId(guideContent.video.youtubeId)) {
    const title = document.createElement("strong");
    title.textContent = guideContent.video.title;
    const copy = document.createElement("span");
    copy.textContent = "YouTube will receive data only after you choose to load the video.";
    const button = document.createElement("button");
    button.className = "button button-primary video-consent-button";
    button.type = "button";
    button.textContent = "Load video from YouTube";
    button.addEventListener("click", () => {
      const iframe = document.createElement("iframe");
      iframe.src = `https://www.youtube-nocookie.com/embed/${guideContent.video.youtubeId}?autoplay=1`;
      iframe.title = guideContent.video.title;
      iframe.allow = "accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share";
      iframe.allowFullscreen = true;
      container.replaceChildren(iframe);
      container.classList.add("has-video");
    });
    const privacy = document.createElement("a");
    privacy.className = "inline-link";
    privacy.href = "../privacy/#youtube";
    privacy.textContent = "Privacy information";
    container.append(title, copy, button, privacy);
    return;
  }

  const title = document.createElement("strong");
  title.textContent = "Video guide coming soon";
  const copy = document.createElement("span");
  copy.textContent = "This will become the quickest way to see the complete build and setup process.";
  container.append(title, copy);
}

function renderHardwareTable() {
  const headerRow = document.querySelector("#hardware-header-row");
  for (const board of guideContent.boards) {
    const header = document.createElement("th");
    header.scope = "col";
    header.dataset.board = board.id;
    header.append(createMediaSlot({ file: board.image, alt: board.imageAlt }, "square"));
    const name = document.createElement("strong");
    name.textContent = board.name;
    header.append(name);
    headerRow.append(header);
  }

  const body = document.querySelector("#hardware-table-body");
  for (const row of guideContent.hardwareRows) {
    const tableRow = document.createElement("tr");
    const label = document.createElement("th");
    label.scope = "row";
    label.textContent = row.label;
    tableRow.append(label);

    for (const board of guideContent.boards) {
      const cell = document.createElement("td");
      const cellValue = board[row.key];
      const copy = document.createElement("p");
      copy.textContent = cellValue.text;
      cell.append(copy);
      if (cellValue.action) {
        cell.append(createExternalLink(cellValue.action));
      }
      tableRow.append(cell);
    }
    body.append(tableRow);
  }
}

function renderScreenshotCards(containerId, screenshots) {
  const container = document.querySelector(containerId);
  for (const screenshot of screenshots) {
    const article = document.createElement("article");
    article.className = "screenshot-card";
    article.append(createMediaSlot(screenshot));
    const body = document.createElement("div");
    body.className = "screenshot-copy";
    const title = document.createElement("h3");
    title.textContent = screenshot.title;
    const description = document.createElement("p");
    description.textContent = screenshot.description;
    body.append(title, description);
    article.append(body);
    container.append(article);
  }
}

renderVideo();
renderHardwareTable();
renderScreenshotCards("#display-screenshots", guideContent.displayScreenshots);
renderScreenshotCards("#web-screenshots", guideContent.webScreenshots);
