import logging
from functools import cmp_to_key

import ctk
import numpy
import qt
import vtk
import vtkITK
from slicer.i18n import tr as _

import slicer

from DICOMLib import DICOMPlugin
from DICOMLib import DICOMLoadable
from DICOMLib import DICOMUtils
from DICOMLib import DICOMExportScalarVolume


#
# This is the plugin to handle translation of scalar volumes
# from DICOM files into MRML nodes.  It follows the DICOM module's
# plugin architecture.
#


class DICOMScalarVolumePluginClass(DICOMPlugin):
    """ScalarVolume specific interpretation code"""

    def __init__(self, spacingEpsilon=1e-2, orientationEpsilon=1e-6):
        """
        :param spacingEpsilon: Tolerance for numeric comparisons for spacing vectors. Defaults to 1.e-2.
        :param orientationEpsilon: Tolerance for numeric comparisons for orientation vectors. Defaults to 1.e-6, following ITK numeric comparison logic.
        """

        super().__init__()
        self.loadType = _("Scalar Volume")
        self.spacingEpsilon = spacingEpsilon
        self.orientationEpsilon = orientationEpsilon
        self.acquisitionModeling = None
        self.defaultStudyID = "SLICER10001"  # TODO: What should be the new study ID?

        self.tags["sopClassUID"] = "0008,0016"
        self.tags["photometricInterpretation"] = "0028,0004"
        self.tags["seriesDescription"] = "0008,103e"
        self.tags["seriesUID"] = "0020,000E"
        self.tags["seriesNumber"] = "0020,0011"
        self.tags["position"] = "0020,0032"
        self.tags["orientation"] = "0020,0037"
        self.tags["pixelData"] = "7fe0,0010"
        self.tags["seriesInstanceUID"] = "0020,000E"
        self.tags["acquisitionNumber"] = "0020,0012"
        self.tags["imageType"] = "0008,0008"
        self.tags["contentTime"] = "0008,0033"
        self.tags["triggerTime"] = "0018,1060"
        self.tags["diffusionGradientOrientation"] = "0018,9089"
        self.tags["imageOrientationPatient"] = "0020,0037"
        self.tags["numberOfFrames"] = "0028,0008"
        self.tags["instanceUID"] = "0008,0018"
        self.tags["windowCenter"] = "0028,1050"
        self.tags["windowWidth"] = "0028,1051"
        self.tags["rows"] = "0028,0010"
        self.tags["columns"] = "0028,0011"

    @staticmethod
    def readerApproaches():
        """Available reader implementations.  First entry is initial default.
        Note: the settings file stores the index of the user's selected reader
        approach, so if new approaches are added the should go at the
        end of the list.
        """
        return ["GDCM with DCMTK fallback", "DCMTK", "GDCM", "Archetype"]

    @staticmethod
    def settingsPanelEntry(panel, parent):
        """Create a settings panel entry for this plugin class.
        It is added to the DICOM panel of the application settings
        by the DICOM module.
        """
        formLayout = qt.QFormLayout(parent)

        readersComboBox = qt.QComboBox()
        for approach in DICOMScalarVolumePluginClass.readerApproaches():
            readersComboBox.addItem(approach)
        readersComboBox.toolTip = _("Preferred back end.  Archetype was used by default in Slicer before June of 2017."
                                    " Change this setting if data that previously loaded stops working (and report an issue).")
        formLayout.addRow(_("DICOM reader approach:"), readersComboBox)
        panel.registerProperty(
            "DICOM/ScalarVolume/ReaderApproach", readersComboBox,
            "currentIndex", str(qt.SIGNAL("currentIndexChanged(int)")))

        importFormatsComboBox = ctk.ctkComboBox()
        importFormatsComboBox.toolTip = _("Enable adding non-linear transform to regularize images acquired irregular geometry:"
                                          " non-rectilinear grid (such as tilted gantry CT acquisitions) and non-uniform slice spacing."
                                          " If no regularization is applied then image may appear distorted if it was acquired with irregular geometry.")

        importFormatsComboBox.addItem(_("default (apply regularization transform)"), "default")
        importFormatsComboBox.addItem(_("none"), "none")
        importFormatsComboBox.addItem(_("apply regularization transform"), "transform")
        importFormatsComboBox.addItem(_("harden regularization transform"), "hardenTransform")

        importFormatsComboBox.currentIndex = 0
        formLayout.addRow(_("Acquisition geometry regularization:"), importFormatsComboBox)
        panel.registerProperty(
            "DICOM/ScalarVolume/AcquisitionGeometryRegularization", importFormatsComboBox,
            "currentUserDataAsString", str(qt.SIGNAL("currentIndexChanged(int)")),
            _("DICOM examination settings"), ctk.ctkSettingsPanel.OptionRequireRestart)
        # DICOM examination settings are cached so we need to restart to make sure changes take effect

        allowLoadingByTimeCheckBox = qt.QCheckBox()
        allowLoadingByTimeCheckBox.toolTip = _("Offer loading of individual slices or group of slices"
                                               " that were acquired at a specific time (content or trigger time)."
                                               " If this option is enabled then a large number of loadable items may be"
                                               " displayed in the Advanced section of DICOM browser.")
        formLayout.addRow(_("Allow loading subseries by time:"), allowLoadingByTimeCheckBox)
        allowLoadingByTimeMapper = ctk.ctkBooleanMapper(allowLoadingByTimeCheckBox, "checked", str(qt.SIGNAL("toggled(bool)")))
        panel.registerProperty(
            "DICOM/ScalarVolume/AllowLoadingByTime", allowLoadingByTimeMapper,
            "valueAsInt", str(qt.SIGNAL("valueAsIntChanged(int)")),
            _("DICOM examination settings"), ctk.ctkSettingsPanel.OptionRequireRestart)
        # DICOM examination settings are cached so we need to restart to make sure changes take effect

    @staticmethod
    def compareVolumeNodes(volumeNode1, volumeNode2):
        """
        Given two mrml volume nodes, return true of the numpy arrays have identical data
        and other metadata matches.  Returns empty string on match, otherwise
        a string with a list of differences separated by newlines.
        """
        volumesLogic = slicer.modules.volumes.logic()
        comparison = ""
        comparison += volumesLogic.CompareVolumeGeometry(volumeNode1, volumeNode2)
        image1 = volumeNode1.GetImageData()
        image2 = volumeNode2.GetImageData()
        if image1.GetScalarType() != image2.GetScalarType():
            comparison += _("First volume is {imageScalarType1}, but second is {imageScalarType2}").format(
                imageScalarType1=image1.GetScalarTypeAsString(), imageScalarType2=image2.GetScalarTypeAsString())
        array1 = slicer.util.array(volumeNode1.GetID())
        array2 = slicer.util.array(volumeNode2.GetID())
        if not numpy.all(array1 == array2):
            comparison += _("Pixel data mismatch") + "\n"
        return comparison

    def hardenAcquisitionGeometryRegularization(self):
        settings = qt.QSettings()
        return settings.value("DICOM/ScalarVolume/AcquisitionGeometryRegularization", "default") == "hardenTransform"

    def acquisitionGeometryRegularizationEnabled(self):
        settings = qt.QSettings()
        return settings.value("DICOM/ScalarVolume/AcquisitionGeometryRegularization", "default") != "none"

    def allowLoadingByTime(self):
        settings = qt.QSettings()
        return int(settings.value("DICOM/ScalarVolume/AllowLoadingByTime", "0")) != 0

    def examineForImport(self, fileLists):
        """Returns a sorted list of DICOMLoadable instances
        corresponding to ways of interpreting the
        fileLists parameter (list of file lists).
        """
        loadables = []
        for files in fileLists:
            cachedLoadables = self.getCachedLoadables(files)
            if cachedLoadables:
                loadables += cachedLoadables
            else:
                loadablesForFiles = self.examineFiles(files)
                loadables += loadablesForFiles
                self.cacheLoadables(files, loadablesForFiles)

        # sort the loadables by series number if possible
        loadables.sort(key=cmp_to_key(lambda x, y: self.seriesSorter(x, y)))

        return loadables

    def cleanNodeName(self, value):
        cleanValue = value
        cleanValue = cleanValue.replace("|", "-")
        cleanValue = cleanValue.replace("/", "-")
        cleanValue = cleanValue.replace("\\", "-")
        cleanValue = cleanValue.replace("*", "(star)")
        cleanValue = cleanValue.replace("\\", "-")
        return cleanValue

    def tagValueToVector(self, value):
        return numpy.array([float(element) for element in value.split("\\")])

    def examineFiles(self, files):
        """Returns a list of DICOMLoadable instances
        corresponding to ways of interpreting the
        files parameter.
        """

        seriesUID = slicer.dicomDatabase.fileValue(files[0], self.tags["seriesUID"])
        seriesName = self.defaultSeriesNodeName(seriesUID)

        # default loadable includes all files for series
        allFilesLoadable = DICOMLoadable()
        allFilesLoadable.files = files
        allFilesLoadable.name = self.cleanNodeName(seriesName)
        allFilesLoadable.tooltip = _("{count} files, first file: {filename}").format(count=len(allFilesLoadable.files), filename=allFilesLoadable.files[0])
        allFilesLoadable.selected = True
        # add it to the list of loadables later, if pixel data is available in at least one file

        # make subseries volumes based on tag differences
        subseriesTags = [
            "seriesInstanceUID",
            "acquisitionNumber",
            # GE volume viewer and Siemens Axiom CBCT systems put an overview (localizer) slice and all the reconstructed slices
            # in one series, using two different image types. Splitting based on image type allows loading of these volumes
            # (loading the series without localizer).
            "imageType",
            "imageOrientationPatient",
            "diffusionGradientOrientation",
        ]

        if self.allowLoadingByTime():
            subseriesTags.append("contentTime")
            subseriesTags.append("triggerTime")

        # Values for these tags will only be enumerated (value itself will not be part of the loadable name)
        # because the vale itself is usually too long and complicated to be displayed to users
        subseriesTagsToEnumerateValues = [
            "seriesInstanceUID",
            "imageOrientationPatient",
            "diffusionGradientOrientation",
        ]

        # Subset of the tags specified in the `subseriesTags` list
        # that should be compared numerically (rather than using the
        # built-in string comparison). This is done when checking if
        # the associated value has already been appended to the
        # `subseriesValues[tag]` list.
        vectorTags = [
            "imageOrientationPatient",
            "diffusionGradientOrientation",
        ]

        #
        # first, look for subseries within this series
        # - build a list of files for each unique value
        #   of each tag
        #
        subseriesFiles = {}
        subseriesValues = {}
        for file in allFilesLoadable.files:
            # check for subseries values
            for tag in subseriesTags:
                value = slicer.dicomDatabase.fileValue(file, self.tags[tag])
                value = value.replace(",", "_")  # remove commas so it can be used as an index

                if tag not in subseriesValues:
                    subseriesValues[tag] = []

                if tag in vectorTags:
                    if value != "":
                        vector = self.tagValueToVector(value)

                        found = False
                        for subseriesValue in subseriesValues[tag]:
                            subseriesVector = self.tagValueToVector(subseriesValue)
                            # vector numerical comparison by absolute difference as the ITK logic.
                            # Reference:
                            #   Class: ITK/Modules/Numerics/Optimizersv4/include/itkObjectToObjectMetric.hxx
                            #   Method: VerifyDisplacementFieldSizeAndPhysicalSpace
                            #   URL: https://github.com/InsightSoftwareConsortium/ITK/blob/v5.4rc02/Modules/Numerics/Optimizersv4/include/itkObjectToObjectMetric.hxx#L507-L510.

                            if numpy.allclose(vector, subseriesVector, rtol=0.0, atol=self.orientationEpsilon):
                                found = True
                                value = subseriesValue
                                break

                        if not found:
                            subseriesValues[tag].append(value)
                elif value not in subseriesValues[tag]:
                    subseriesValues[tag].append(value)
                if (tag, value) not in subseriesFiles:
                    subseriesFiles[tag, value] = []
                subseriesFiles[tag, value].append(file)

        loadables = []

        # Pixel data is available, so add the default loadable to the output
        loadables.append(allFilesLoadable)

        #
        # second, for any tags that have more than one value, create a new
        # virtual series
        #
        subseriesCount = 0
        # List of loadables that look like subseries that contain the full series except a single frame
        probableLocalizerFreeLoadables = []
        for tag in subseriesTags:
            if len(subseriesValues[tag]) > 1:
                subseriesCount += 1
                for valueIndex, value in enumerate(subseriesValues[tag]):
                    # default loadable includes all files for series
                    loadable = DICOMLoadable()
                    loadable.files = subseriesFiles[tag, value]
                    # value can be a long string (and it will be used for generating node name)
                    # therefore use just an index instead
                    if tag in subseriesTagsToEnumerateValues:
                        loadable.name = seriesName + " - %s %d" % (tag, valueIndex + 1)
                    else:
                        loadable.name = seriesName + f" - {tag} {value}"
                    loadable.name = self.cleanNodeName(loadable.name)
                    loadable.tooltip = _("{count} files, grouped by {tag} = {value}. First file: {filename}").format(
                        count=len(loadable.files), tag=tag, value=value, filename=loadable.files[0])
                    loadable.selected = False
                    loadables.append(loadable)
                    if len(subseriesValues[tag]) == 2:
                        otherValue = subseriesValues[tag][1 - valueIndex]
                        if len(subseriesFiles[tag, value]) > 1 and len(subseriesFiles[tag, otherValue]) == 1:
                            # this looks like a subseries without a localizer image
                            probableLocalizerFreeLoadables.append(loadable)

        # remove any files from loadables that don't have pixel data (no point sending them to ITK for reading)
        # also remove DICOM SEG, since it is not handled by ITK readers
        newLoadables = []
        for loadable in loadables:
            newFiles = []
            excludedLoadable = False
            for file in loadable.files:
                if slicer.dicomDatabase.fileValueExists(file, self.tags["pixelData"]):
                    newFiles.append(file)
                if slicer.dicomDatabase.fileValue(file, self.tags["sopClassUID"]) == "1.2.840.10008.5.1.4.1.1.66.4":
                    excludedLoadable = True
                    if "DICOMSegmentationPlugin" not in slicer.modules.dicomPlugins:
                        logging.warning("Please install Quantitative Reporting extension to enable loading of DICOM Segmentation objects")
                elif slicer.dicomDatabase.fileValue(file, self.tags["sopClassUID"]) == "1.2.840.10008.5.1.4.1.1.481.3":
                    excludedLoadable = True
                    if "DicomRtImportExportPlugin" not in slicer.modules.dicomPlugins:
                        logging.warning("Please install SlicerRT extension to enable loading of DICOM RT Structure Set objects")
            if len(newFiles) > 0 and not excludedLoadable:
                loadable.files = newFiles
                loadable.grayscale = ("MONOCHROME" in slicer.dicomDatabase.fileValue(newFiles[0], self.tags["photometricInterpretation"]))
                newLoadables.append(loadable)
            elif excludedLoadable:
                continue
            else:
                # here all files in have no pixel data, so they might be
                # secondary capture images which will read, so let's pass
                # them through with a warning and low confidence
                loadable.warning += _("There is no pixel data attribute for the DICOM objects, but they might be readable as secondary capture images.")
                loadable.confidence = 0.2
                loadable.grayscale = ("MONOCHROME" in slicer.dicomDatabase.fileValue(loadable.files[0], self.tags["photometricInterpretation"]))
                newLoadables.append(loadable)
        loadables = newLoadables

        #
        # now for each series and subseries, sort the images
        # by position and check for consistency
        # then adjust confidence values based on warnings
        #
        for loadable in loadables:
            loadable.files, distances, loadable.warning = DICOMUtils.getSortedImageFiles(loadable.files, self.spacingEpsilon)

        loadablesBetterThanAllFiles = []
        if allFilesLoadable.warning != "":
            for probableLocalizerFreeLoadable in probableLocalizerFreeLoadables:
                if probableLocalizerFreeLoadable.warning == "":
                    # localizer-free loadables are better then all files, if they don't have warning
                    loadablesBetterThanAllFiles.append(probableLocalizerFreeLoadable)
            if not loadablesBetterThanAllFiles and subseriesCount == 1:
                # there was a sorting warning and
                # only one kind of subseries, so it's probably correct
                # to have lower confidence in the default all-files version.
                for loadable in loadables:
                    if loadable != allFilesLoadable and loadable.warning == "":
                        loadablesBetterThanAllFiles.append(loadable)

        # if there are loadables that are clearly better then all files, then use those (otherwise use all files loadable)
        preferredLoadables = loadablesBetterThanAllFiles if loadablesBetterThanAllFiles else [allFilesLoadable]
        # reduce confidence and deselect all non-preferred loadables
        for loadable in loadables:
            if loadable in preferredLoadables:
                loadable.selected = True
            else:
                loadable.selected = False
                loadable.confidence = min(loadable.confidence, 0.45)

        return loadables

    def seriesSorter(self, x, y):
        """
        Returns -1, 0, 1 for sorting of strings like: "400: series description"

        Works for DICOMLoadable or other objects with name attribute
        """
        if not (hasattr(x, "name") and hasattr(y, "name")):
            return 0
        xName = x.name
        yName = y.name
        try:
            xNumber = int(xName[: xName.index(":")])
            yNumber = int(yName[: yName.index(":")])
        except ValueError:
            return 0
        cmp = xNumber - yNumber
        return cmp

    #
    # different ways to load a set of dicom files:
    # - Logic: relies on the same loading mechanism used
    #   by the File->Add Data dialog in the Slicer GUI.
    #   This uses vtkITK under the hood with GDCM as
    #   the default loader.
    # - DCMTK: explicitly uses the DCMTKImageIO
    # - GDCM: explicitly uses the GDCMImageIO
    #

    def loadFilesWithArchetype(self, files, name):
        """Load files in the traditional Slicer manner
        using the volume logic helper class
        and the vtkITK archetype helper code
        """
        fileList = vtk.vtkStringArray()
        for f in files:
            fileList.InsertNextValue(f)
        volumesLogic = slicer.modules.volumes.logic()
        return volumesLogic.AddArchetypeScalarVolume(files[0], name, 0, fileList)

    def loadFilesWithSeriesReader(self, imageIOName, files, name, grayscale=True):
        """Explicitly use the named imageIO to perform the loading"""

        if grayscale:
            reader = vtkITK.vtkITKArchetypeImageSeriesScalarReader()
        else:
            if len(files) > 1:
                reader = vtkITK.vtkITKArchetypeImageSeriesVectorReaderSeries()
            else:
                reader = vtkITK.vtkITKArchetypeImageSeriesVectorReaderFile()
        reader.SetArchetype(files[0])
        for f in files:
            reader.AddFileName(f)
        reader.SetSingleFile(0)
        reader.SetOutputScalarTypeToNative()
        reader.SetDesiredCoordinateOrientationToNative()
        reader.SetUseNativeOriginOn()
        if imageIOName == "GDCM":
            reader.SetDICOMImageIOApproachToGDCM()
        elif imageIOName == "DCMTK":
            reader.SetDICOMImageIOApproachToDCMTK()
        else:
            raise Exception("Invalid imageIOName of %s" % imageIOName)
        logging.info("Loading with imageIOName: %s" % imageIOName)
        reader.Update()

        slicer.modules.reader = reader
        if reader.GetErrorCode() != vtk.vtkErrorCode.NoError:
            errorStrings = (imageIOName, vtk.vtkErrorCode.GetStringFromErrorCode(reader.GetErrorCode()))
            logging.error("Could not read scalar volume using %s approach.  Error is: %s" % errorStrings)
            return

        imageChangeInformation = vtk.vtkImageChangeInformation()
        imageChangeInformation.SetInputConnection(reader.GetOutputPort())
        imageChangeInformation.SetOutputSpacing(1, 1, 1)
        imageChangeInformation.SetOutputOrigin(0, 0, 0)
        imageChangeInformation.Update()

        name = slicer.mrmlScene.GenerateUniqueName(name)
        if grayscale:
            volumeNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLScalarVolumeNode", name)
        else:
            volumeNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLVectorVolumeNode", name)
        volumeNode.SetAndObserveImageData(imageChangeInformation.GetOutputDataObject(0))
        slicer.vtkMRMLVolumeArchetypeStorageNode.SetMetaDataDictionaryFromReader(volumeNode, reader)
        volumeNode.SetRASToIJKMatrix(reader.GetRasToIjkMatrix())
        volumeNode.CreateDefaultDisplayNodes()

        slicer.modules.DICOMInstance.reader = reader
        slicer.modules.DICOMInstance.imageChangeInformation = imageChangeInformation

        return volumeNode

    def setVolumeNodeProperties(self, volumeNode, loadable):
        """After the scalar volume has been loaded, populate the node
        attributes and display node with values extracted from the dicom instances
        """
        if volumeNode:
            #
            # create subject hierarchy items for the loaded series
            #
            self.addSeriesInSubjectHierarchy(loadable, volumeNode)

            # File for each slice. Will be reversed if reading a left-handed volume.
            fileList = loadable.files

            ijkToRAS = vtk.vtkMatrix4x4()
            volumeNode.GetIJKToRASMatrix(ijkToRAS)
            if not slicer.vtkMRMLVolumeNode.IsIJKCoordinateSystemRightHanded(ijkToRAS):
                # This volume is in left-handed coordinate system. Reorder the slices to make it right-handed
                # to ensure all computational algorithms work well (e.g., avoid turning generated surfaces inside out).
                logging.info(f"Reverse slice order to force volume '{volumeNode.GetName()}' ({volumeNode.GetID()}) to use right-handed IJK coordinate system")
                slicer.vtkMRMLVolumeNode.ReverseSliceOrder(volumeNode.GetImageData(), ijkToRAS)
                volumeNode.SetIJKToRASMatrix(ijkToRAS)
                # Reorder the file list so that i-th slice corresponds to i-th DICOM.instanceUIDs attribute value
                fileList = list(reversed(fileList))

            #
            # add list of DICOM instance UIDs to the volume node
            # corresponding to the loaded files
            #
            instanceUIDs = ""
            for file in fileList:
                uid = slicer.dicomDatabase.fileValue(file, self.tags["instanceUID"])
                if uid == "":
                    uid = "Unknown"
                instanceUIDs += uid + " "
            instanceUIDs = instanceUIDs[:-1]  # strip last space
            volumeNode.SetAttribute("DICOM.instanceUIDs", instanceUIDs)

            #
            # add list of DICOM instance numbers to the volume node
            # corresponding to the loaded files
            #
            instanceNumbers = []
            for file in fileList:
                instanceNumber = slicer.dicomDatabase.fileValue(file, "0020,0013")  # Instance Number tag
                instanceNumbers.append(instanceNumber if instanceNumber else "?")
            volumeNode.SetAttribute("DICOM.instanceNumbers", " ".join(instanceNumbers))

            # Choose a file in the middle of the series as representative frame,
            # because that is more likely to contain the object of interest than the first or last frame.
            # This is important for example for getting a relevant window/center value for the series.
            file = fileList[int(len(fileList) / 2)]
            uid = slicer.dicomDatabase.fileValue(file, self.tags["instanceUID"])

            #
            # automatically select the volume to display
            #
            appLogic = slicer.app.applicationLogic()
            selNode = appLogic.GetSelectionNode()
            selNode.SetActiveVolumeID(volumeNode.GetID())
            appLogic.PropagateVolumeSelection()

            #
            # apply window/level from DICOM if available (the first pair that is found)
            #   Note: There can be multiple presets (multiplicity 1-n) in the standard [1]. We have
            #   a way to put these into the display node [2], so they can be selected in the Volumes
            #   module.
            #   [1] https://medical.nema.org/medical/dicom/current/output/html/part06.html
            #   [2] https://github.com/Slicer/Slicer/blob/3bfa2fc2b310d41c09b7a9e8f8f6c4f43d3bd1e2/Libs/MRML/Core/vtkMRMLScalarVolumeDisplayNode.h#L172
            #
            try:
                displayNode = volumeNode.GetScalarVolumeDisplayNode()
                if displayNode:
                    # Retrieve the window center and window width values from the DICOM file
                    windowCenterValues = slicer.dicomDatabase.fileValue(file, self.tags["windowCenter"])
                    windowWidthValues = slicer.dicomDatabase.fileValue(file, self.tags["windowWidth"])
                    if windowCenterValues or windowWidthValues:
                        # Split the values by the backslash character to handle multiple values
                        windowCenterList = windowCenterValues.split("\\")
                        windowWidthList = windowWidthValues.split("\\")

                        # Ensure both lists have the same length
                        if len(windowCenterList) != len(windowWidthList):
                            raise ValueError(f"Number of window width/center values do not match ({windowWidthList}/{windowCenterList}).")

                        # Iterate over the pairs and add window level presets
                        for windowCenter, windowWidth in zip(windowCenterList, windowWidthList, strict=True):
                            # Convert the values to float
                            windowCenter = float(windowCenter)
                            windowWidth = float(windowWidth)

                            # Add the window level preset to the display node
                            displayNode.AddWindowLevelPreset(windowWidth, windowCenter)
                            logging.debug(f"DICOM window/level ({windowCenter}/{windowWidth}) set to volume '{volumeNode.GetName()}' from SOP instance {uid}.")
                        displayNode.SetWindowLevelFromPreset(0)
                    else:
                        logging.debug(f"No window/level found for volume '{volumeNode.GetName()}' in DICOM tags of SOP instance {uid}.")
                else:
                    logging.info(f"No display node for volume '{volumeNode.GetName()}': cannot store window/level found in DICOM tags")
            except ValueError as e:
                logging.error(f"Failed to parse window center/width values for volume '{volumeNode.GetName()}' SOP instance: {uid}. Details: {str(e)}")

            sopClassUID = slicer.dicomDatabase.fileValue(file, self.tags["sopClassUID"])

            # initialize color lookup table
            modality = self.mapSOPClassUIDToModality(sopClassUID)
            if modality == "PT":
                displayNode = volumeNode.GetDisplayNode()
                if displayNode:
                    displayNode.SetAndObserveColorNodeID(slicer.modules.colors.logic().GetPETColorNodeID(slicer.vtkMRMLPETProceduralColorNode.PETheat))

            # initialize quantity and units codes
            (quantity, units) = self.mapSOPClassUIDToDICOMQuantityAndUnits(sopClassUID)
            if quantity is not None:
                volumeNode.SetVoxelValueQuantity(quantity)
            if units is not None:
                volumeNode.SetVoxelValueUnits(units)

    def loadWithMultipleLoaders(self, loadable):
        """Load using multiple paths (for testing)"""
        volumeNode = self.loadFilesWithArchetype(loadable.files, loadable.name + "-archetype")
        self.setVolumeNodeProperties(volumeNode, loadable)
        volumeNode = self.loadFilesWithSeriesReader("GDCM", loadable.files, loadable.name + "-gdcm", loadable.grayscale)
        self.setVolumeNodeProperties(volumeNode, loadable)
        volumeNode = self.loadFilesWithSeriesReader("DCMTK", loadable.files, loadable.name + "-dcmtk", loadable.grayscale)
        self.setVolumeNodeProperties(volumeNode, loadable)

        return volumeNode

    def load(self, loadable, readerApproach=None):
        """Load the select as a scalar volume using desired approach"""
        # first, determine which reader approach the user prefers
        if not readerApproach:
            readerIndex = slicer.util.settingsValue("DICOM/ScalarVolume/ReaderApproach", 0, converter=int)
            readerApproach = DICOMScalarVolumePluginClass.readerApproaches()[readerIndex]
        # second, try to load with the selected approach
        if readerApproach == "Archetype":
            volumeNode = self.loadFilesWithArchetype(loadable.files, loadable.name)
        elif readerApproach == "GDCM with DCMTK fallback":
            volumeNode = self.loadFilesWithSeriesReader("GDCM", loadable.files, loadable.name, loadable.grayscale)
            if not volumeNode:
                volumeNode = self.loadFilesWithSeriesReader("DCMTK", loadable.files, loadable.name, loadable.grayscale)
        else:
            volumeNode = self.loadFilesWithSeriesReader(readerApproach, loadable.files, loadable.name, loadable.grayscale)
        # third, transfer data from the dicom instances into the appropriate Slicer data containers
        self.setVolumeNodeProperties(volumeNode, loadable)

        # examine the loaded volume and if needed create a new transform
        # that makes the loaded volume match the DICOM coordinates of
        # the individual frames.  Save the class instance so external
        # code such as the DICOMReaders test can introspect to validate.

        if volumeNode:
            self.acquisitionModeling = self.AcquisitionModeling()
            self.acquisitionModeling.createAcquisitionTransform(volumeNode,
                                                                addAcquisitionTransformIfNeeded=self.acquisitionGeometryRegularizationEnabled(),
                                                                hardenAcquisitionTransform=self.hardenAcquisitionGeometryRegularization())

        return volumeNode

    def examineForExport(self, subjectHierarchyItemID):
        """Return a list of DICOMExportable instances that describe the
        available techniques that this plugin offers to convert MRML
        data into DICOM data
        """
        # cannot export if there is no data node or the data node is not a volume
        shn = slicer.vtkMRMLSubjectHierarchyNode.GetSubjectHierarchyNode(slicer.mrmlScene)
        dataNode = shn.GetItemDataNode(subjectHierarchyItemID)
        if dataNode is None or not dataNode.IsA("vtkMRMLScalarVolumeNode"):
            return []

        # Define basic properties of the exportable
        exportable = slicer.qSlicerDICOMExportable()
        exportable.name = self.loadType
        exportable.tooltip = _("Creates a series of DICOM files from scalar volumes")
        exportable.subjectHierarchyItemID = subjectHierarchyItemID
        exportable.pluginClass = self.__module__
        exportable.confidence = 0.5  # There could be more specialized volume types

        # Define required tags and default values
        exportable.setTag("SeriesDescription", "No series description")  # no_tr (all these are DICOM tag names, not displayed to user)
        exportable.setTag("Modality", "CT")
        exportable.setTag("Manufacturer", "Unknown manufacturer")
        exportable.setTag("Model", "Unknown model")
        exportable.setTag("StudyDate", "")
        exportable.setTag("StudyTime", "")
        exportable.setTag("StudyInstanceUID", "")
        exportable.setTag("SeriesDate", "")
        exportable.setTag("SeriesTime", "")
        exportable.setTag("ContentDate", "")
        exportable.setTag("ContentTime", "")
        exportable.setTag("SeriesNumber", "301")
        exportable.setTag("SeriesInstanceUID", "")
        exportable.setTag("FrameOfReferenceUID", "")

        return [exportable]

    def export(self, exportables):
        for exportable in exportables:
            # Get volume node to export
            shNode = slicer.vtkMRMLSubjectHierarchyNode.GetSubjectHierarchyNode(slicer.mrmlScene)
            if shNode is None:
                error = _("Invalid subject hierarchy")
                logging.error(error)
                return error
            volumeNode = shNode.GetItemDataNode(exportable.subjectHierarchyItemID)
            if volumeNode is None or not volumeNode.IsA("vtkMRMLScalarVolumeNode"):
                error = _("Series '{itemName}' cannot be exported").format(itemName=shNode.GetItemName(exportable.subjectHierarchyItemID))
                logging.error(error)
                return error

            # Get output directory and create a subdirectory. This is necessary
            # to avoid overwriting the files in case of multiple exportables, as
            # naming of the DICOM files is static
            directoryName = "ScalarVolume_" + str(exportable.subjectHierarchyItemID)
            directoryDir = qt.QDir(exportable.directory)
            directoryDir.mkpath(directoryName)
            directoryDir.cd(directoryName)
            directory = directoryDir.absolutePath()
            logging.info("Export scalar volume '" + volumeNode.GetName() + "' to directory " + directory)

            # Get study and patient items
            studyItemID = shNode.GetItemParent(exportable.subjectHierarchyItemID)
            if not studyItemID:
                error = _("Unable to get study for series '{volumeName}'").format(volumeName=volumeNode.GetName())
                logging.error(error)
                return error
            patientItemID = shNode.GetItemParent(studyItemID)
            if not patientItemID:
                error = _("Unable to get patient for series '{volumeName}'").format(volumeName=volumeNode.GetName())
                logging.error(error)
                return error

            # Assemble tags dictionary for volume export
            tags = {}
            tags["Patient Name"] = exportable.tag(slicer.vtkMRMLSubjectHierarchyConstants.GetDICOMPatientNameTagName())
            tags["Patient ID"] = exportable.tag(slicer.vtkMRMLSubjectHierarchyConstants.GetDICOMPatientIDTagName())
            tags["Patient Birth Date"] = exportable.tag(slicer.vtkMRMLSubjectHierarchyConstants.GetDICOMPatientBirthDateTagName())
            tags["Patient Sex"] = exportable.tag(slicer.vtkMRMLSubjectHierarchyConstants.GetDICOMPatientSexTagName())
            tags["Patient Comments"] = exportable.tag(slicer.vtkMRMLSubjectHierarchyConstants.GetDICOMPatientCommentsTagName())
            tags["Study ID"] = exportable.tag(slicer.vtkMRMLSubjectHierarchyConstants.GetDICOMStudyIDTagName())
            tags["Study Date"] = exportable.tag(slicer.vtkMRMLSubjectHierarchyConstants.GetDICOMStudyDateTagName())
            tags["Study Time"] = exportable.tag(slicer.vtkMRMLSubjectHierarchyConstants.GetDICOMStudyTimeTagName())
            tags["Study Description"] = exportable.tag(slicer.vtkMRMLSubjectHierarchyConstants.GetDICOMStudyDescriptionTagName())
            tags["Modality"] = exportable.tag("Modality")
            tags["Manufacturer"] = exportable.tag("Manufacturer")
            tags["Model"] = exportable.tag("Model")
            tags["Series Description"] = exportable.tag("SeriesDescription")
            tags["Series Number"] = exportable.tag("SeriesNumber")
            tags["Series Date"] = exportable.tag("SeriesDate")
            tags["Series Time"] = exportable.tag("SeriesTime")
            tags["Content Date"] = exportable.tag("ContentDate")
            tags["Content Time"] = exportable.tag("ContentTime")

            tags["Study Instance UID"] = exportable.tag("StudyInstanceUID")
            tags["Series Instance UID"] = exportable.tag("SeriesInstanceUID")
            tags["Frame of Reference UID"] = exportable.tag("FrameOfReferenceUID")

            # Generate any missing but required UIDs
            if not tags["Study Instance UID"]:
                import pydicom as dicom

                tags["Study Instance UID"] = dicom.uid.generate_uid()
            if not tags["Series Instance UID"]:
                import pydicom as dicom

                tags["Series Instance UID"] = dicom.uid.generate_uid()
            if not tags["Frame of Reference UID"]:
                import pydicom as dicom

                tags["Frame of Reference UID"] = dicom.uid.generate_uid()

            # Use the default Study ID if none is specified
            if not tags["Study ID"]:
                tags["Study ID"] = self.defaultStudyID

            # Validate tags
            if tags["Modality"] == "":
                error = _("Empty modality for series '{volumeName}'").format(volumeName=volumeNode.GetName())
                logging.error(error)
                return error

            seriesInstanceUID = tags["Series Instance UID"]
            if seriesInstanceUID:
                # Make sure we don't use a series instance UID that already exists (it would mix in more slices into an existing series,
                # which is very unlikely that users would want).
                db = slicer.dicomDatabase
                studyInstanceUID = db.studyForSeries(seriesInstanceUID)
                if studyInstanceUID:
                    # This seriesInstanceUID is already found in the database
                    if len(seriesInstanceUID) > 25:
                        seriesInstanceUID = seriesInstanceUID[:20] + "..."
                    error = _("A series already exists in the database by SeriesInstanceUID {seriesInstanceUID}.").format(seriesInstanceUID=seriesInstanceUID)
                    logging.error(error)
                    return error

            # TODO: more tag checks

            # Perform export
            exporter = DICOMExportScalarVolume(tags["Study ID"], volumeNode, tags, directory)
            if not exporter.export():
                return _("Creating DICOM files from scalar volume failed. See the application log for details.")

        # Success
        return ""

    class AcquisitionModeling:
        """Code for representing and analyzing acquisition properties in slicer
        This is an internal class of the DICOMScalarVolumePluginClass so that
        it can be used here and from within the DICOMReaders test.

        TODO: This code work on legacy single frame DICOM images that have position and orientation
        flags in each instance (not on multiframe with per-frame positions).
        """

        def __init__(self, cornerEpsilon=1e-3, zeroEpsilon=1e-6):
            """cornerEpsilon sets the threshold for the amount of difference between the
            vtkITK generated volume geometry vs the DICOM geometry.  Any spatial dimension with
            a difference larger than cornerEpsilon will trigger the addition of a grid transform.
            Any difference less than zeroEpsilon is assumed to be numerical error.
            """
            self.cornerEpsilon = cornerEpsilon
            self.zeroEpsilon = zeroEpsilon

        def gridTransformFromCorners(self, volumeNode, sourceCorners, targetCorners):
            """Create a grid transform that maps between the current and the desired corners."""
            # sanity check
            columns, rows, slices = volumeNode.GetImageData().GetDimensions()
            cornerShape = (slices, 2, 2, 3)
            if not (sourceCorners.shape == cornerShape and targetCorners.shape == cornerShape):
                raise Exception("Corner shapes do not match volume dimensions %s, %s, %s" %
                                (sourceCorners.shape, targetCorners.shape, cornerShape))

            # create the grid transform node
            gridTransform = slicer.vtkMRMLGridTransformNode()
            gridTransform.SetName(slicer.mrmlScene.GenerateUniqueName(volumeNode.GetName() + " acquisition transform"))
            slicer.mrmlScene.AddNode(gridTransform)

            # place grid transform in the same subject hierarchy folder as the volume node
            shNode = slicer.vtkMRMLSubjectHierarchyNode.GetSubjectHierarchyNode(slicer.mrmlScene)
            volumeParentItemId = shNode.GetItemParent(shNode.GetItemByDataNode(volumeNode))
            shNode.SetItemParent(shNode.GetItemByDataNode(gridTransform), volumeParentItemId)

            # create a grid transform with one vector at the corner of each slice
            # the transform is in the same space and orientation as the volume node
            gridImage = vtk.vtkImageData()
            gridImage.SetOrigin(*volumeNode.GetOrigin())
            gridImage.SetDimensions(2, 2, slices)
            sourceSpacing = volumeNode.GetSpacing()
            gridImage.SetSpacing(sourceSpacing[0] * columns, sourceSpacing[1] * rows, sourceSpacing[2])
            gridImage.AllocateScalars(vtk.VTK_DOUBLE, 3)
            transform = slicer.vtkOrientedGridTransform()
            directionMatrix = vtk.vtkMatrix4x4()
            volumeNode.GetIJKToRASDirectionMatrix(directionMatrix)
            transform.SetGridDirectionMatrix(directionMatrix)
            transform.SetDisplacementGridData(gridImage)
            gridTransform.SetAndObserveTransformToParent(transform)
            volumeNode.SetAndObserveTransformNodeID(gridTransform.GetID())

            # populate the grid so that each corner of each slice
            # is mapped from the source corner to the target corner
            displacements = slicer.util.arrayFromGridTransform(gridTransform)
            for sliceIndex in range(slices):
                for row in range(2):
                    for column in range(2):
                        displacements[sliceIndex][row][column] = targetCorners[sliceIndex][row][column] - sourceCorners[sliceIndex][row][column]

        def sliceCornersFromDICOM(self, volumeNode):
            """Calculate the RAS position of each of the four corners of each
            slice of a volume node based on the dicom headers

            Note: PixelSpacing is row spacing followed by column spacing [1] (i.e. vertical then horizontal)
            while ImageOrientationPatient is row cosines then column cosines [2] (i.e. horizontal then vertical).
            [1] https://dicom.nema.org/medical/dicom/current/output/html/part03.html#sect_10.7.1.1
            [2] https://dicom.nema.org/medical/dicom/current/output/html/part03.html#sect_C.7.6.2
            """
            spacingTag = "0028,0030"
            positionTag = "0020,0032"
            orientationTag = "0020,0037"

            columns, rows, slices = volumeNode.GetImageData().GetDimensions()
            corners = numpy.zeros(shape=[slices, 2, 2, 3])
            instanceUIDsAttribute = volumeNode.GetAttribute("DICOM.instanceUIDs")
            uids = instanceUIDsAttribute.split() if instanceUIDsAttribute else []
            if len(uids) != slices:
                # There is no uid for each slice, so most likely all frames are in a single file
                # or maybe there is a problem with the sequence
                logging.warning("Cannot get DICOM slice positions for volume " + volumeNode.GetName())
                return None
            for sliceIndex in range(slices):
                uid = uids[sliceIndex]
                # get slice geometry from instance
                positionString = slicer.dicomDatabase.instanceValue(uid, positionTag)
                orientationString = slicer.dicomDatabase.instanceValue(uid, orientationTag)
                spacingString = slicer.dicomDatabase.instanceValue(uid, spacingTag)
                if positionString == "" or orientationString == "" or spacingString == "":
                    logging.warning("No geometry information available for DICOM data, skipping corner calculations")
                    return None

                position = numpy.array(list(map(float, positionString.split("\\"))))
                orientation = list(map(float, orientationString.split("\\")))
                rowOrientation = numpy.array(orientation[:3])
                columnOrientation = numpy.array(orientation[3:])
                spacing = numpy.array(list(map(float, spacingString.split("\\"))))
                # map from LPS to RAS
                lpsToRAS = numpy.array([-1, -1, 1])
                position *= lpsToRAS
                rowOrientation *= lpsToRAS
                columnOrientation *= lpsToRAS
                rowVector = columns * spacing[1] * rowOrientation  # dicom PixelSpacing is between rows first, then columns
                columnVector = rows * spacing[0] * columnOrientation
                # apply the transform to the four corners
                for column in range(2):
                    for row in range(2):
                        corners[sliceIndex][row][column] = position
                        corners[sliceIndex][row][column] += column * rowVector
                        corners[sliceIndex][row][column] += row * columnVector
            return corners

        def sliceCornersFromIJKToRAS(self, volumeNode):
            """Calculate the RAS position of each of the four corners of each
            slice of a volume node based on the ijkToRAS matrix of the volume node
            """
            ijkToRAS = vtk.vtkMatrix4x4()
            volumeNode.GetIJKToRASMatrix(ijkToRAS)
            columns, rows, slices = volumeNode.GetImageData().GetDimensions()
            corners = numpy.zeros(shape=[slices, 2, 2, 3])
            for sliceIndex in range(slices):
                for column in range(2):
                    for row in range(2):
                        corners[sliceIndex][row][column] = numpy.array(ijkToRAS.MultiplyPoint([column * columns, row * rows, sliceIndex, 1])[:3])
            return corners

        def cornersToWorld(self, volumeNode, corners):
            """Map corners through the volumeNodes transform to world
            This can be used to confirm that an acquisition transform has correctly
            mapped the slice corners to match the dicom acquisition.
            """
            columns, rows, slices = volumeNode.GetImageData().GetDimensions()
            worldCorners = numpy.zeros(shape=[slices, 2, 2, 3])
            for slice in range(slices):
                for row in range(2):
                    for column in range(2):
                        volumeNode.TransformPointToWorld(corners[slice, row, column], worldCorners[slice, row, column])
            return worldCorners

        def createAcquisitionTransform(self, volumeNode, addAcquisitionTransformIfNeeded=True, hardenAcquisitionTransform=False):
            """Creates the actual transform if needed.
            Slice corners are cached for inspection by tests
            """
            self.originalCorners = self.sliceCornersFromIJKToRAS(volumeNode)
            self.targetCorners = self.sliceCornersFromDICOM(volumeNode)
            if self.originalCorners is None or self.targetCorners is None:
                # can't create transform without corner information
                return
            maxError = (abs(self.originalCorners - self.targetCorners)).max()

            if maxError > self.cornerEpsilon:
                warningText = f"Irregular volume geometry detected (maximum error of {maxError:g} mm is above tolerance threshold of {self.cornerEpsilon:g} mm)."
                if addAcquisitionTransformIfNeeded:
                    logging.warning(warningText + "  Adding acquisition transform to regularize geometry.")
                    self.gridTransformFromCorners(volumeNode, self.originalCorners, self.targetCorners)
                    self.fixedCorners = self.cornersToWorld(volumeNode, self.originalCorners)
                    if not numpy.allclose(self.fixedCorners, self.targetCorners):
                        raise Exception("Acquisition transform didn't fix slice corners!")
                    if hardenAcquisitionTransform:
                        volumeNode.HardenTransform()
                else:
                    logging.warning(warningText + "  Regularization transform is not added, as the option is disabled.")
            elif maxError > 0 and maxError > self.zeroEpsilon:
                logging.debug("Irregular volume geometry detected, but maximum error is within tolerance" +
                              f" (maximum error of {maxError:g} mm, tolerance threshold is {self.cornerEpsilon:g} mm).")


#
# DICOMScalarVolumePlugin
#


class DICOMScalarVolumePlugin:
    """
    This class is the 'hook' for slicer to detect and recognize the plugin
    as a loadable scripted module
    """

    def __init__(self, parent):
        # no tr (these strings are not translated because they are only visible for developers)
        parent.title = "DICOM Scalar Volume Plugin"
        parent.categories = ["Developer Tools.DICOM Plugins"]
        parent.contributors = ["Steve Pieper (Isomics Inc.), Csaba Pinter (Queen's)"]
        parent.helpText = """
    Plugin to the DICOM Module to parse and load scalar volumes
    from DICOM files.
    No module interface here, only in the DICOM module
    """
        parent.acknowledgementText = """
    This DICOM Plugin was developed by
    Steve Pieper, Isomics, Inc.
    and was partially funded by NIH grant 3P41RR013218.
    """

        # don't show this module - it only appears in the DICOM module
        parent.hidden = True

        # Add this extension to the DICOM module's list for discovery when the module
        # is created.  Since this module may be discovered before DICOM itself,
        # create the list if it doesn't already exist.
        try:
            slicer.modules.dicomPlugins
        except AttributeError:
            slicer.modules.dicomPlugins = {}
        slicer.modules.dicomPlugins["DICOMScalarVolumePlugin"] = DICOMScalarVolumePluginClass
