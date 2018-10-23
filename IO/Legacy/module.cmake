vtk_module(vtkIOLegacy
  GROUPS
    StandAlone
  TEST_DEPENDS
    vtkFiltersAMR
    vtkImagingCore
    vtkInteractionStyle
    vtkRenderingOpenGL2
    vtkTestingRendering
  KIT
    vtkIO
  DEPENDS
    vtkCommonCore
    vtkCommonDataModel
    vtkCommonExecutionModel
    vtkIOCore
  PRIVATE_DEPENDS
    vtkCommonMisc
    vtksys
  )