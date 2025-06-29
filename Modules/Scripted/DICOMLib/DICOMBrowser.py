import copy
import logging
import os

import ctk
import qt

import slicer
from slicer.util import VTKObservationMixin
from slicer.util import settingsValue, toBool

from slicer.i18n import tr as _

import DICOMLib
from DICOMLib import DICOMUtils


#########################################################
#
#
comment = """

DICOMWidgets are helper classes to build an interface
to manage DICOM data in the context of slicer.

This code is slicer-specific and relies on the slicer python module
for elements like slicer.dicomDatabase and slicer.mrmlScene

"""


#
#########################################################


class SlicerDICOMBrowser(VTKObservationMixin, qt.QWidget):
    """Implement the Qt window showing details and possible
    operations to perform on the selected dicom list item.
    This is a helper used in the DICOMWidget class.
    """

    closed = qt.Signal()  # Invoked when the dicom widget is closed using the close method

    def __init__(self, dicomBrowser=None, parent="mainWindow"):
        VTKObservationMixin.__init__(self)
        qt.QWidget.__init__(self, slicer.util.mainWindow() if parent == "mainWindow" else parent)

        # Allow customization of DICOM plugins
        self.enabledPluginClassNames = None  # can be set to a list containing enabled DICOM plugin class names, or set to None for default set
        self.pluginInstances = {}  # custom DICOM plugin instances can be added to override the default set

        self.fileLists = []
        self.extensionCheckPending = False

        self.settings = qt.QSettings()

        self.dicomBrowser = dicomBrowser if dicomBrowser is not None else slicer.app.createDICOMBrowserForMainDatabase()

        # Add ctkVisualDICOMBrowser
        self.dicomVisualBrowser = ctk.ctkDICOMVisualBrowserWidget()
        self.dicomVisualBrowser.findChild(ctk.ctkCollapsibleGroupBox, "ActionsCollapsibleGroupBox").hide()
        if settingsValue("DICOM/thumbnailsSize", False) == "large":
            self.dicomVisualBrowser.thumbnailSize = ctk.ctkDICOMStudyItemWidget.Large
        elif settingsValue("DICOM/thumbnailsSize", False) == "medium":
            self.dicomVisualBrowser.thumbnailSize = ctk.ctkDICOMStudyItemWidget.Medium
        elif settingsValue("DICOM/thumbnailsSize", False) == "small":
            self.dicomVisualBrowser.thumbnailSize = ctk.ctkDICOMStudyItemWidget.Small

        if settingsValue("DICOM/detailedLogging", False, converter=toBool):
            ctk.ctk.setDICOMLogLevel(ctk.ctkErrorLogLevel.Debug)

        self.browserPersistent = settingsValue("DICOM/BrowserPersistent", False, converter=toBool)
        self.advancedView = settingsValue("DICOM/advancedView", 0, converter=int)
        self.horizontalTables = settingsValue("DICOM/horizontalTables", 0, converter=int)

        self.setup()

        self.dicomBrowser.connect("directoryImported()", self.onDirectoryImported)
        self.dicomBrowser.connect("sendRequested(QStringList)", self.onSend)

        # Load when double-clicked on an item in the browser
        self.dicomBrowser.dicomTableManager().connect("patientsDoubleClicked(QModelIndex)", self.patientStudySeriesDoubleClicked)
        self.dicomBrowser.dicomTableManager().connect("studiesDoubleClicked(QModelIndex)", self.patientStudySeriesDoubleClicked)
        self.dicomBrowser.dicomTableManager().connect("seriesDoubleClicked(QModelIndex)", self.patientStudySeriesDoubleClicked)

        self.dicomVisualBrowser.setDatabaseDirectory(self.dicomBrowser.databaseDirectory)
        self.dicomVisualBrowser.seriesRetrieved.connect(self.onSeriesRetrieved)
        self.dicomVisualBrowser.connect("sendRequested(QStringList)", self.onSend)

    def onSeriesRetrieved(self, seriesInstanceUIDs):
        seriesList = [str(seriesInstanceUID) for seriesInstanceUID in seriesInstanceUIDs]
        if seriesList is None or not seriesList:
            return
        nodes = DICOMUtils.loadSeriesByUID(seriesList)
        if len(nodes) > 0 and not settingsValue("DICOM/BrowserPersistent", False, converter=toBool):
            self.close()

    def open(self):
        self.show()

    def close(self):
        self.hide()
        self.closed.emit()

    def onSend(self, fileList):
        if len(fileList):
            sendDialog = DICOMLib.DICOMSendDialog(fileList, self)

    def createNewDatabaseDirectory(self):
        if self.useExpertimentalVisualDICOMBrowser:
            self.dicomVisualBrowser.createNewDatabaseDirectory()
        else:
            self.dicomBrowser.createNewDatabaseDirectory()

    def setDatabaseDirectory(self, databaseDirectory):
        self.dicomBrowser.databaseDirectory = databaseDirectory
        self.dicomVisualBrowser.databaseDirectory = databaseDirectory

    def importDirectoryMode(self):
        if self.useExpertimentalVisualDICOMBrowser:
            return self.dicomVisualBrowser.ImportDirectoryMode
        else:
            return self.dicomBrowser.ImportDirectoryMode

    def setImportDirectoryMode(self, mode):
        self.dicomVisualBrowser.ImportDirectoryMode = mode
        self.dicomBrowser.ImportDirectoryMode = mode

    def importFolder(self):
        if self.useExpertimentalVisualDICOMBrowser:
            self.dicomVisualBrowser.openImportDialog()
        else:
            self.dicomBrowser.openImportDialog()

    def importDirectories(self, directoriesToAdd):
        if self.useExpertimentalVisualDICOMBrowser:
            self.dicomVisualBrowser.importDirectories(directoriesToAdd)
        else:
            self.dicomBrowser.importDirectories(directoriesToAdd)

    def importDirectory(self, dicomFilesDirectory):
        if self.useExpertimentalVisualDICOMBrowser:
            self.dicomVisualBrowser.importDirectory(dicomFilesDirectory, self.importDirectoryMode())
        else:
            self.dicomBrowser.importDirectory(dicomFilesDirectory, self.importDirectoryMode())

    def toggleBrowsers(self, useExpertimentalVisualDICOMBrowser):
        self.useExpertimentalVisualDICOMBrowser = useExpertimentalVisualDICOMBrowser
        self.settings.setValue("DICOM/UseExpertimentalVisualDICOMBrowser", bool(useExpertimentalVisualDICOMBrowser))

        self.dicomVisualBrowser.visible = self.useExpertimentalVisualDICOMBrowser
        self.dicomBrowser.visible = not self.useExpertimentalVisualDICOMBrowser
        self.loadableTableFrame.visible = (not self.useExpertimentalVisualDICOMBrowser) and self.advancedView
        self.actionButtonsFrame.visible = not self.useExpertimentalVisualDICOMBrowser
        if self.useExpertimentalVisualDICOMBrowser and self.dicomVisualBrowser.patientsTabWidget().count == 0:
            self.dicomVisualBrowser.onShowPatients()

    def setup(self, showPreview=False):
        """
        main window is a frame with widgets from the app
        widget repacked into it along with slicer-specific
        extra widgets
        """

        self.setWindowTitle(_("DICOM Browser"))
        self.setLayout(qt.QVBoxLayout())

        self.dicomBrowser.databaseDirectorySelectorVisible = False
        self.dicomBrowser.toolbarVisible = False
        self.dicomBrowser.sendActionVisible = True
        self.dicomBrowser.databaseDirectorySettingsKey = slicer.dicomDatabaseDirectorySettingsKey
        self.dicomBrowser.dicomTableManager().dynamicTableLayout = False
        horizontal = self.settings.setValue("DICOM/horizontalTables", 0)
        self.dicomBrowser.dicomTableManager().tableOrientation = qt.Qt.Horizontal if horizontal else qt.Qt.Vertical
        self.layout().addWidget(self.dicomBrowser)

        self.dicomVisualBrowser.sendActionVisible = True
        # Fix rendering groupbox
        self.dicomVisualBrowser.serverSettingsGroupBox().setChecked(True)
        self.dicomVisualBrowser.serverSettingsGroupBox().setChecked(False)
        self.dicomVisualBrowser.databaseDirectorySettingsKey = slicer.dicomDatabaseDirectorySettingsKey
        self.layout().addWidget(self.dicomVisualBrowser)

        self.userFrame = qt.QWidget()
        self.preview = qt.QWidget()

        #
        # preview related column
        #
        self.previewLayout = qt.QVBoxLayout()
        if showPreview:
            self.previewLayout.addWidget(self.preview)
        else:
            self.preview.hide()

        #
        # action related column (interacting with slicer)
        #
        self.loadableTableFrame = qt.QWidget()
        self.loadableTableFrame.setMaximumHeight(200)
        self.loadableTableLayout = qt.QVBoxLayout(self.loadableTableFrame)
        self.layout().addWidget(self.loadableTableFrame)

        self.loadableTableLayout.addWidget(self.userFrame)
        self.userFrame.hide()

        self.loadableTable = DICOMLoadableTable(self.userFrame)
        self.loadableTable.itemChanged.connect(self.onLoadableTableItemChanged)

        #
        # button row for action column
        #
        self.actionButtonsFrame = qt.QWidget()
        self.actionButtonsFrame.setMaximumHeight(40)
        self.actionButtonsFrame.objectName = "ActionButtonsFrame"
        self.layout().addWidget(self.actionButtonsFrame)

        self.actionButtonLayout = qt.QHBoxLayout()
        self.actionButtonsFrame.setLayout(self.actionButtonLayout)

        self.uncheckAllButton = qt.QPushButton(_("Uncheck All"))
        self.actionButtonLayout.addWidget(self.uncheckAllButton)
        self.uncheckAllButton.connect("clicked()", self.uncheckAllLoadables)

        self.actionButtonLayout.addStretch(0.05)

        self.examineButton = qt.QPushButton(_("Examine"))
        self.examineButton.setSizePolicy(qt.QSizePolicy.Expanding, qt.QSizePolicy.Fixed)
        self.actionButtonLayout.addWidget(self.examineButton)
        self.examineButton.enabled = False
        self.examineButton.connect("clicked()", self.examineForLoading)

        self.loadButton = qt.QPushButton(_("Load"))
        self.loadButton.setSizePolicy(qt.QSizePolicy.Expanding, qt.QSizePolicy.Fixed)
        self.loadButton.toolTip = _("Load selected items into the scene")
        self.actionButtonLayout.addWidget(self.loadButton)
        self.loadButton.connect("clicked()", self.loadCheckedLoadables)

        self.actionButtonLayout.addStretch(0.05)

        self.advancedViewButton = qt.QCheckBox(_("Advanced"))
        self.advancedViewButton.objectName = "AdvancedViewCheckBox"
        self.actionButtonLayout.addWidget(self.advancedViewButton)
        self.advancedViewButton.checked = self.advancedView
        self.advancedViewButton.toggled.connect(self.onAdvancedViewButton)

        if self.advancedView:
            self.loadableTableFrame.visible = True
        else:
            self.loadableTableFrame.visible = False
            self.examineButton.visible = False
            self.uncheckAllButton.visible = False

        #
        # Series selection
        #
        self.dicomBrowser.dicomTableManager().connect("seriesSelectionChanged(QStringList)", self.onSeriesSelected)

        #
        # Loadable table widget (advanced)
        # DICOM Plugins selection widget is moved to module panel
        #
        self.loadableTableLayout.addWidget(self.loadableTable)
        self.updateButtonStates()

    def updateButtonStates(self):
        if self.advancedView:
            # self.loadButton.enabled =         loadEnabled = loadEnabled or loadablesByPlugin[plugin] != []
            loadablesChecked = self.loadableTable.getNumberOfCheckedItems() > 0
            self.loadButton.enabled = loadablesChecked
            self.examineButton.enabled = len(self.fileLists) != 0
            self.uncheckAllButton.enabled = loadablesChecked
        else:
            # seriesSelected  = self.dicomBrowser.dicomTableManager().seriesTable().tableView().selectedIndexes()
            self.loadButton.enabled = self.fileLists

    def onDirectoryImported(self):
        """The dicom browser will emit multiple directoryImported
        signals during the same operation, so we collapse them
        into a single check for compatible extensions.
        """
        if not hasattr(slicer.app, "extensionsManagerModel"):
            # Slicer may not be built with extensions manager support
            return
        if not self.extensionCheckPending:
            self.extensionCheckPending = True

            def timerCallback():
                # Prompting for extension may be undesirable in custom applications.
                # DICOM/PromptForExtensions key can be used to disable this feature.
                promptForExtensionsEnabled = settingsValue("DICOM/PromptForExtensions", True, converter=toBool)
                if promptForExtensionsEnabled:
                    self.promptForExtensions()
                self.extensionCheckPending = False

            qt.QTimer.singleShot(0, timerCallback)

    def promptForExtensions(self):
        extensionsToOffer = self.checkForExtensions()
        if len(extensionsToOffer) != 0:
            displayedTypeDescriptions = []
            for extension in extensionsToOffer:
                typeDescription = extension["typeDescription"]
                if typeDescription not in displayedTypeDescriptions:
                    displayedTypeDescriptions.append(typeDescription)
            displayedExtensionNames = []
            for extension in extensionsToOffer:
                extensionName = extension["name"]
                if extensionName not in displayedExtensionNames:
                    # only display each extension name only once
                    displayedExtensionNames.append(extensionName)

            message = ""
            if len(displayedTypeDescriptions) == 1:
                message += _("The following data type is in your database:")
            else:
                message += _("The following data types are in your database:")
            message += " <b>" + ", ".join(displayedTypeDescriptions) + "</b>."

            message += "<p><p>"
            if len(displayedExtensionNames) == 1:
                message += _("The following extension is not installed, but may help you work with this data:")
            else:
                message += _("The following extensions are not installed, but may help you work with this data:")
            message += " <b>" + ", ".join(displayedExtensionNames) + "</b>."

            message += "<p><p>"
            message += _("You can install extensions using the Extensions Manager option from the View menu.")
            slicer.util.infoDisplay(message, parent=self, windowTitle=_("DICOM"))

    def checkForExtensions(self):
        """Check to see if there
        are any registered extensions that might be available to
        help the user work with data in the database.

        1) load extension json description
        2) load info for each series
        3) check if data matches

        then return matches

        See
        https://mantisarchive.slicer.org/view.php?id=4146
        """

        # 1 - load json
        import logging, os, json

        logging.info("Imported a DICOM directory, checking for extensions")
        modulePath = os.path.dirname(slicer.modules.dicom.path)
        extensionDescriptorPath = os.path.join(modulePath, "DICOMExtensions.json")
        try:
            with open(extensionDescriptorPath) as extensionDescriptorFP:
                extensionDescriptor = extensionDescriptorFP.read()
                dicomExtensions = json.loads(extensionDescriptor)
        except:
            logging.error("Cannot access DICOMExtensions.json file")
            return

        # 2 - get series info
        #  - iterate though metadata - should be fast even with large database
        #  - the fileValue call checks the tag cache so it's fast
        modalityTag = "0008,0060"
        sopClassUIDTag = "0008,0016"
        sopClassUIDs = set()
        modalities = set()
        for patient in slicer.dicomDatabase.patients():
            for study in slicer.dicomDatabase.studiesForPatient(patient):
                for series in slicer.dicomDatabase.seriesForStudy(study):
                    instance0 = slicer.dicomDatabase.filesForSeries(series, 1)[0]
                    modality = slicer.dicomDatabase.fileValue(instance0, modalityTag)
                    sopClassUID = slicer.dicomDatabase.fileValue(instance0, sopClassUIDTag)
                    modalities.add(modality)
                    sopClassUIDs.add(sopClassUID)

        # 3 - check if data matches
        extensionsManagerModel = slicer.app.extensionsManagerModel()
        installedExtensions = extensionsManagerModel.installedExtensions
        extensionsToOffer = []
        for extension in dicomExtensions["extensions"]:
            extensionName = extension["name"]
            if extensionName not in installedExtensions:
                tagValues = extension["tagValues"]
                if "Modality" in tagValues:
                    for modality in tagValues["Modality"]:
                        if modality in modalities:
                            extensionsToOffer.append(extension)
                if "SOPClassUID" in tagValues:
                    for sopClassUID in tagValues["SOPClassUID"]:
                        if sopClassUID in sopClassUIDs:
                            extensionsToOffer.append(extension)
        return extensionsToOffer

    def setBrowserPersistence(self, state):
        self.browserPersistent = state
        self.settings.setValue("DICOM/BrowserPersistent", bool(self.browserPersistent))

    def onAdvancedViewButton(self, checked):
        self.advancedView = checked
        advancedWidgets = [self.loadableTableFrame, self.examineButton, self.uncheckAllButton]
        for widget in advancedWidgets:
            widget.visible = self.advancedView
        self.updateButtonStates()

        self.settings.setValue("DICOM/advancedView", int(self.advancedView))

    def onHorizontalViewCheckBox(self):
        horizontal = self.horizontalViewCheckBox.checked
        self.dicomBrowser.dicomTableManager().tableOrientation = qt.Qt.Horizontal if horizontal else qt.Qt.Vertical
        self.settings.setValue("DICOM/horizontalTables", int(horizontal))

    def onSeriesSelected(self, seriesUIDList):
        self.loadableTable.setLoadables([])
        self.fileLists = self.getFileListsForRole(seriesUIDList, "SeriesUIDList")
        self.updateButtonStates()

    def getFileListsForRole(self, uidArgument, role):
        fileLists = []
        if role == "Series":
            fileLists.append(slicer.dicomDatabase.filesForSeries(uidArgument))
        if role == "SeriesUIDList":
            for uid in uidArgument:
                uid = uid.replace("'", "")
                fileLists.append(slicer.dicomDatabase.filesForSeries(uid))
        if role == "Study":
            series = slicer.dicomDatabase.seriesForStudy(uidArgument)
            for serie in series:
                fileLists.append(slicer.dicomDatabase.filesForSeries(serie))
        if role == "Patient":
            studies = slicer.dicomDatabase.studiesForPatient(uidArgument)
            for study in studies:
                series = slicer.dicomDatabase.seriesForStudy(study)
                for serie in series:
                    fileList = slicer.dicomDatabase.filesForSeries(serie)
                    fileLists.append(fileList)
        return fileLists

    def uncheckAllLoadables(self):
        self.loadableTable.uncheckAll()

    def onLoadableTableItemChanged(self, item):
        self.updateButtonStates()

    def examineForLoading(self):
        """For selected plugins, give user the option
        of what to load
        """

        (self.loadablesByPlugin, loadEnabled) = self.getLoadablesFromFileLists(self.fileLists)
        DICOMLib.selectHighestConfidenceLoadables(self.loadablesByPlugin)
        self.loadableTable.setLoadables(self.loadablesByPlugin)
        self.updateButtonStates()

    def getLoadablesFromFileLists(self, fileLists):
        """Take list of file lists, return loadables by plugin dictionary"""

        loadablesByPlugin = {}
        loadEnabled = False

        allFileCount = missingFileCount = 0
        for fileList in fileLists:
            for filePath in fileList:
                allFileCount += 1
                if not os.path.exists(filePath):
                    missingFileCount += 1

        messages = []
        if missingFileCount > 0:
            messages.append(
                _("{missing_file_count} of {total_file_count} selected files listed in the database cannot be found on disk.").format(
                    missing_file_count=missingFileCount, total_file_count=allFileCount))

        if missingFileCount < allFileCount:
            progressDialog = slicer.util.createProgressDialog(parent=self, value=0, maximum=100)

            def progressCallback(progressDialog, progressLabel, progressValue):
                progressDialog.labelText = "\n" + _("Checking {what}").format(what=progressLabel)
                slicer.app.processEvents()
                progressDialog.setValue(progressValue)
                slicer.app.processEvents()
                cancelled = progressDialog.wasCanceled
                return cancelled

            loadablesByPlugin, loadEnabled = DICOMLib.getLoadablesFromFileLists(fileLists,
                                                                                self.enabledPluginClassNames,
                                                                                messages,
                                                                                lambda progressLabel, progressValue, progressDialog=progressDialog: progressCallback(progressDialog, progressLabel, progressValue),
                                                                                self.pluginInstances)

            progressDialog.setParent(None)
            progressDialog.close()

        if messages:
            slicer.util.warningDisplay(
                _("Warning: {messages}").format(messages=" ".join(messages))
                + "\n\n" + _("See python console for error message."),
                windowTitle=_("DICOM"), parent=self)

        return loadablesByPlugin, loadEnabled

    def isFileListInCheckedLoadables(self, fileList):
        for plugin in self.loadablesByPlugin:
            for loadable in self.loadablesByPlugin[plugin]:
                if len(loadable.files) != len(fileList) or len(loadable.files) == 0:
                    continue
                inputFileListCopy = copy.deepcopy(fileList)
                loadableFileListCopy = copy.deepcopy(loadable.files)
                try:
                    inputFileListCopy.sort()
                    loadableFileListCopy.sort()
                except Exception:
                    pass
                isEqual = True
                for pair in zip(inputFileListCopy, loadableFileListCopy, strict=True):
                    if pair[0] != pair[1]:
                        print(f"{pair[0]} != {pair[1]}")
                        isEqual = False
                        break
                if not isEqual:
                    continue
                return True
        return False

    def patientStudySeriesDoubleClicked(self):
        if self.advancedViewButton.checkState() == 0:
            # basic mode
            self.loadCheckedLoadables()
        else:
            # advanced mode, just examine the double-clicked item, do not load
            self.examineForLoading()

    def loadCheckedLoadables(self):
        """Invoke the load method on each plugin for the loadable
        (DICOMLoadable or qSlicerDICOMLoadable) instances that are selected
        """
        if self.advancedViewButton.checkState() == 0:
            self.examineForLoading()

        self.loadableTable.updateSelectedFromCheckstate()

        # TODO: add check that disables all referenced stuff to be considered?
        # get all the references from the checked loadables
        referencedFileLists = []
        for plugin in self.loadablesByPlugin:
            for loadable in self.loadablesByPlugin[plugin]:
                if hasattr(loadable, "referencedInstanceUIDs"):
                    instanceFileList = []
                    for instance in loadable.referencedInstanceUIDs:
                        instanceFile = slicer.dicomDatabase.fileForInstance(instance)
                        if instanceFile != "":
                            instanceFileList.append(instanceFile)
                    if instanceFileList and not self.isFileListInCheckedLoadables(instanceFileList):
                        referencedFileLists.append(instanceFileList)

        # if applicable, find all loadables from the file lists
        loadEnabled = False
        if referencedFileLists:
            (self.referencedLoadables, loadEnabled) = self.getLoadablesFromFileLists(referencedFileLists)

        automaticallyLoadReferences = int(slicer.util.settingsValue("DICOM/automaticallyLoadReferences", qt.QMessageBox.InvalidRole))
        if slicer.app.commandOptions().testingEnabled:
            automaticallyLoadReferences = qt.QMessageBox.No
        if loadEnabled and automaticallyLoadReferences == qt.QMessageBox.InvalidRole:
            self.showReferenceDialogAndProceed()
        elif loadEnabled and automaticallyLoadReferences == qt.QMessageBox.Yes:
            self.addReferencesAndProceed()
        else:
            self.proceedWithReferencedLoadablesSelection()

        return

    def showReferenceDialogAndProceed(self):
        referencesDialog = DICOMReferencesDialog(self, loadables=self.referencedLoadables)
        answer = referencesDialog.exec_()
        if referencesDialog.rememberChoiceAndStopAskingCheckbox.checked is True:
            if answer == qt.QMessageBox.Yes:
                qt.QSettings().setValue("DICOM/automaticallyLoadReferences", qt.QMessageBox.Yes)
            if answer == qt.QMessageBox.No:
                qt.QSettings().setValue("DICOM/automaticallyLoadReferences", qt.QMessageBox.No)
        if answer == qt.QMessageBox.Yes:
            # each check box corresponds to a referenced loadable that was selected by examine;
            # if the user confirmed that reference should be loaded, add it to the self.loadablesByPlugin dictionary
            for plugin in self.referencedLoadables:
                for loadable in [loadable_item for loadable_item in self.referencedLoadables[plugin] if loadable_item.selected]:
                    if referencesDialog.checkboxes[loadable].checked:
                        self.loadablesByPlugin[plugin].append(loadable)
                self.loadablesByPlugin[plugin] = list(set(self.loadablesByPlugin[plugin]))
            self.proceedWithReferencedLoadablesSelection()
        elif answer == qt.QMessageBox.No:
            self.proceedWithReferencedLoadablesSelection()

    def addReferencesAndProceed(self):
        for plugin in self.referencedLoadables:
            for loadable in [loadable_item for loadable_item in self.referencedLoadables[plugin] if loadable_item.selected]:
                self.loadablesByPlugin[plugin].append(loadable)
            self.loadablesByPlugin[plugin] = list(set(self.loadablesByPlugin[plugin]))
        self.proceedWithReferencedLoadablesSelection()

    def proceedWithReferencedLoadablesSelection(self):
        if not self.warnUserIfLoadableWarningsAndProceed():
            return

        progressDialog = slicer.util.createProgressDialog(parent=self, value=0, maximum=100)

        def progressCallback(progressDialog, progressLabel, progressValue):
            progressDialog.labelText = "\n" + _("Loading {what}").format(what=progressLabel)
            slicer.app.processEvents()
            progressDialog.setValue(progressValue)
            slicer.app.processEvents()
            cancelled = progressDialog.wasCanceled
            return cancelled

        qt.QApplication.setOverrideCursor(qt.Qt.WaitCursor)

        messages = []
        loadedNodeIDs = DICOMLib.loadLoadables(self.loadablesByPlugin, messages,
                                               lambda progressLabel, progressValue, progressDialog=progressDialog: progressCallback(progressDialog, progressLabel, progressValue))

        loadedFileParameters = {}
        loadedFileParameters["nodeIDs"] = loadedNodeIDs
        slicer.app.ioManager().emitNewFileLoaded(loadedFileParameters)

        qt.QApplication.restoreOverrideCursor()

        progressDialog.setParent(None)
        progressDialog.close()

        if messages:
            slicer.util.warningDisplay("\n".join(messages), windowTitle=_("DICOM loading"))

        self.onLoadingFinished()

    def warnUserIfLoadableWarningsAndProceed(self):
        warningsInSelectedLoadables = False
        details = ""
        for plugin in self.loadablesByPlugin:
            for loadable in self.loadablesByPlugin[plugin]:
                if loadable.selected and loadable.warning != "":
                    warningsInSelectedLoadables = True
                    logging.warning(_("Warning in DICOM plugin {load_type} when examining loadable {name}: {message}").format(
                        load_type=plugin.loadType, name=loadable.name, message=loadable.warning))
                    details += loadable.name + " [" + plugin.loadType + "]: " + loadable.warning + "\n"
        if warningsInSelectedLoadables:
            warning = _("Warnings detected during load.  Examine data in Advanced mode for details.  Load anyway?")
            if not slicer.util.confirmOkCancelDisplay(warning, parent=self, detailedText=details):
                return False
        return True

    def onLoadingFinished(self):
        if not self.browserPersistent:
            self.close()


class DICOMReferencesDialog(qt.QMessageBox):
    WINDOW_TITLE = _("Referenced datasets found")
    WINDOW_TEXT = _("The loaded DICOM objects contain references to other datasets you did not select for loading."
                    " Please select Yes if you would like to load the following referenced datasets, No if you only want to load the"
                    " originally selected series, or Cancel to abort loading.")

    def __init__(self, parent, loadables):
        super().__init__(parent)
        self.loadables = loadables
        self.checkboxes = dict()
        self.setup()

    def setup(self):
        self._setBasicProperties()
        self._addTextLabel()
        self._addLoadableCheckboxes()
        self.rememberChoiceAndStopAskingCheckbox = qt.QCheckBox(_("Remember choice and stop asking"))
        self.rememberChoiceAndStopAskingCheckbox.toolTip = _("Can be changed later in Application Settings / DICOM")
        self.yesButton = self.addButton(self.Yes)
        self.yesButton.setSizePolicy(qt.QSizePolicy(qt.QSizePolicy.Expanding, qt.QSizePolicy.Preferred))
        self.noButton = self.addButton(self.No)
        self.noButton.setSizePolicy(qt.QSizePolicy(qt.QSizePolicy.Expanding, qt.QSizePolicy.Preferred))
        self.cancelButton = self.addButton(self.Cancel)
        self.cancelButton.setSizePolicy(qt.QSizePolicy(qt.QSizePolicy.Expanding, qt.QSizePolicy.Preferred))
        self.layout().addWidget(self.yesButton, 3, 0, 1, 1)
        self.layout().addWidget(self.noButton, 3, 1, 1, 1)
        self.layout().addWidget(self.cancelButton, 3, 2, 1, 1)
        self.layout().addWidget(self.rememberChoiceAndStopAskingCheckbox, 2, 0, 1, 3)

    def _setBasicProperties(self):
        self.layout().setSpacing(9)
        self.setWindowTitle(self.WINDOW_TITLE)
        fontMetrics = qt.QFontMetrics(qt.QApplication.font(self))
        try:
            self.setMinimumWidth(fontMetrics.horizontalAdvance(self.WINDOW_TITLE))
        except AttributeError:
            # Support Qt < 5.11 lacking QFontMetrics::horizontalAdvance()
            self.setMinimumWidth(fontMetrics.width(self.WINDOW_TITLE))

    def _addTextLabel(self):
        label = qt.QLabel(self.WINDOW_TEXT)
        label.wordWrap = True
        self.layout().addWidget(label, 0, 0, 1, 3)

    def _addLoadableCheckboxes(self):
        self.checkBoxGroupBox = qt.QGroupBox(_("References"))
        self.checkBoxGroupBox.setLayout(qt.QFormLayout())
        for plugin in self.loadables:
            for loadable in [loadable_item for loadable_item in self.loadables[plugin] if loadable_item.selected]:
                checkBoxText = loadable.name + " (" + plugin.loadType + ") "
                cb = qt.QCheckBox(checkBoxText, self)
                cb.checked = True
                cb.setSizePolicy(qt.QSizePolicy(qt.QSizePolicy.Expanding, qt.QSizePolicy.Preferred))
                self.checkboxes[loadable] = cb
                self.checkBoxGroupBox.layout().addWidget(cb)
        self.layout().addWidget(self.checkBoxGroupBox, 1, 0, 1, 3)


class DICOMLoadableTable(qt.QTableWidget):
    """Implement the Qt code for a table of
    selectable slicer data to be made from
    the given dicom files
    """

    def __init__(self, parent, width=350, height=100):
        super().__init__(parent)
        self.setMinimumHeight(height)
        self.setMinimumWidth(width)
        self.loadables = {}
        self.setLoadables([])
        self.configure()
        slicer.app.connect("aboutToQuit()", self.deleteLater)

    def getNumberOfCheckedItems(self):
        return sum(1 for row in range(self.rowCount) if self.item(row, 0).checkState() == qt.Qt.Checked)

    def configure(self):
        self.setColumnCount(3)
        self.setHorizontalHeaderLabels([_("DICOM Data"), _("Reader"), _("Warnings")])
        self.setSelectionBehavior(qt.QTableView.SelectRows)
        self.horizontalHeader().setSectionResizeMode(qt.QHeaderView.Stretch)
        self.horizontalHeader().setSectionResizeMode(0, qt.QHeaderView.Interactive)
        self.horizontalHeader().setSectionResizeMode(1, qt.QHeaderView.ResizeToContents)
        self.horizontalHeader().setSectionResizeMode(2, qt.QHeaderView.Stretch)
        self.horizontalScrollMode = qt.QAbstractItemView.ScrollPerPixel

    def addLoadableRow(self, loadable, row, reader):
        self.insertRow(row)
        self.loadables[row] = loadable
        item = qt.QTableWidgetItem(loadable.name)
        self.setItem(row, 0, item)
        self.setCheckState(item, loadable)
        self.addReaderColumn(item, reader, row)
        self.addWarningColumn(item, loadable, row)

    def setCheckState(self, item, loadable):
        item.setCheckState(qt.Qt.Checked if loadable.selected else qt.Qt.Unchecked)
        item.setToolTip(loadable.tooltip)

    def addReaderColumn(self, item, reader, row):
        if not reader:
            return
        readerItem = qt.QTableWidgetItem(reader)
        readerItem.setFlags(readerItem.flags() ^ qt.Qt.ItemIsEditable)
        self.setItem(row, 1, readerItem)
        readerItem.setToolTip(item.toolTip())

    def addWarningColumn(self, item, loadable, row):
        warning = loadable.warning if loadable.warning else ""
        warnItem = qt.QTableWidgetItem(warning)
        warnItem.setFlags(warnItem.flags() ^ qt.Qt.ItemIsEditable)
        self.setItem(row, 2, warnItem)
        item.setToolTip(item.toolTip() + "\n" + warning)
        warnItem.setToolTip(item.toolTip())

    def setLoadables(self, loadablesByPlugin):
        """Load the table widget with a list
        of volume options (of class DICOMVolume)
        """
        self.clearContents()
        self.setRowCount(0)
        self.loadables = {}

        # For each plugin, keep only a single loadable selected for the same file set
        # to prevent loading same data multiple times.
        for plugin in loadablesByPlugin:
            for thisLoadableId in range(len(loadablesByPlugin[plugin])):
                for prevLoadableId in range(0, thisLoadableId):
                    thisLoadable = loadablesByPlugin[plugin][thisLoadableId]
                    prevLoadable = loadablesByPlugin[plugin][prevLoadableId]
                    # fileDifferences will contain all the files that only present in one or the other list (or tuple)
                    fileDifferences = set(thisLoadable.files).symmetric_difference(set(prevLoadable.files))
                    if (not fileDifferences) and (prevLoadable.selected):
                        thisLoadable.selected = False
                        break

        row = 0
        for selectState in (True, False):
            for plugin in loadablesByPlugin:
                for loadable in loadablesByPlugin[plugin]:
                    if loadable.selected == selectState:
                        self.addLoadableRow(loadable, row, plugin.loadType)
                        row += 1

        self.setVerticalHeaderLabels(row * [""])

    def uncheckAll(self):
        for row in range(self.rowCount):
            item = self.item(row, 0)
            item.setCheckState(False)

    def updateSelectedFromCheckstate(self):
        for row in range(self.rowCount):
            item = self.item(row, 0)
            self.loadables[row].selected = item.checkState() != 0
            # updating the names
            self.loadables[row].name = item.text()
