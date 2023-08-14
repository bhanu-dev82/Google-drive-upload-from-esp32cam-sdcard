function doPost(e) {
  var data = e.postData.contents; // Get the raw POST data directly
  var mimeType = "image/jpeg"; // Adjust the mimeType if necessary
  var folderName = "ESP32-CAM"; // Folder name where the images will be stored

  var folder;
  try {
    folder = DriveApp.getFoldersByName(folderName).next();
  } catch (e) {
    folder = DriveApp.createFolder(folderName);
  }

  var imageBlob = Utilities.newBlob(Utilities.base64Decode(data), mimeType);
  var timestamp = new Date().getTime();
  var fileName = "image_" + timestamp + ".jpg"; // File name with timestamp

  var file = folder.createFile(imageBlob.setName(fileName));

  var response = {
    success: true,
    message: "Image uploaded successfully.",
    imageUrl: file.getUrl()
  };

  return ContentService.createTextOutput(JSON.stringify(response))
    .setMimeType(ContentService.MimeType.JSON);
}
