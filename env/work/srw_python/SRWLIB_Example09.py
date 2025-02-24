###########################################################################
# SRWLIB Example#9: Simulating propagation of a Gaussian X-ray beam through a Beamline containing an Imperfect Mirror
# v 0.01
#############################################################################

from __future__ import print_function #Python 2.7 compatibility
from srwlib import *
import os
import sys
import math

print('SRWLIB Python Example # 9:')
print('Simulating propagation of a Coherent Gaussian X-ray beam through a Beamline containing Imperfect Mirrors')

#**********************Auxiliary Functions
#Read data comumns from ASCII file:
def AuxReadInDataColumns(filePath, nCol, strSep):
    f = open(filePath, 'r')
    resCols = []
    for iCol in range(nCol):
        resCols.append([])

    curLine = f.readline()
    while len(curLine) > 0:
        curLineParts = curLine.split(strSep)
        for iCol in range(nCol):
            if(iCol < len(curLineParts)):
                resCols[iCol].append(float(curLineParts[iCol]))
        curLine = f.readline()
    f.close()
    return resCols #attn: returns lists, not arrays!

#Setup Transmission optical element with 1D heght profile data
def AuxTransmAddSurfHeightProfile(optSlopeErr, heightProfData, dim, ang):
    argHeightProfData = heightProfData[0]
    valHeightProfData = heightProfData[1]
    sinAng = math.sin(ang)
    npData = len(heightProfData[0])
    
    #xStep = optSlopeErr.rx/(optSlopeErr.nx - 1)
    #yStep = optSlopeErr.ry/(optSlopeErr.ny - 1)
    #y = optSlopeErr.y - 0.5*optSlopeErr.ry

    auxMesh = optSlopeErr.mesh
    xStep = (auxMesh.xFin - auxMesh.xStart)/(auxMesh.nx - 1)
    yStep = (auxMesh.yFin - auxMesh.yStart)/(auxMesh.ny - 1)

    y = auxMesh.yStart
    hApprox = 0
    ipStart = 0
    #for iy in range(optSlopeErr.ny):
    for iy in range(auxMesh.ny):
        if('y' in dim):
            hApprox = 0
            y1 = argHeightProfData[ipStart]*sinAng
            for i in range(ipStart + 1, npData):
                y2 = argHeightProfData[i]*sinAng
                if((y1 <= y) and (y < y2)):
                    hApprox = ((valHeightProfData[i] - valHeightProfData[i-1])/((argHeightProfData[i] - argHeightProfData[i-1])*sinAng))*(y - y1) + valHeightProfData[i-1]
                    #print(ipStart, i, iy, y1, y, y2, argHeightProfData[i-1], argHeightProfData[i], valHeightProfData[i-1], valHeightProfData[i], hApprox)
                    ipStart = i - 1
                    break
                y1 = y2

        #x = optSlopeErr.x - 0.5*optSlopeErr.rx
        x = auxMesh.xStart
        
        #for ix in range(optSlopeErr.nx):
        for ix in range(auxMesh.nx):
            if('x' in dim):
                if(ix == 0): ipStart = 0
                hApprox = 0
                x1 = argHeightProfData[ipStart]*sinAng
                for i in range(ipStart + 1, npData):
                    x2 = argHeightProfData[i]*sinAng
                    if((x1 <= x) and (x < x2)):
                        hApprox = ((valHeightProfData[i] - valHeightProfData[i-1])/((argHeightProfData[i] - argHeightProfData[i-1])*sinAng))*(x - x1) + valHeightProfData[i-1]
                        ipStart = i - 1
                        break
                    x1 = x2
            #ofst = 2*ix + (2*optSlopeErr.nx)*iy
            ofst = 2*ix + (2*auxMesh.nx)*iy

            optSlopeErr.arTr[ofst] = 1. #Amplitude Transmission
            optSlopeErr.arTr[ofst + 1] = 0. #Optical Path Difference
            if(hApprox != 0):
                optSlopeErr.arTr[ofst + 1] = -2*sinAng*hApprox #Optical Path Difference (to check sign!)
                #print(ix, iy, optSlopeErr.arTr[ofst + 1])
            x += xStep
        y += yStep

#**********************Source and Initial Wavefront Input Parameters and Structures:

strDataFolderName = 'data_example_09' #data sub-folder name

GsnBm = SRWLGsnBm() #Gaussian Beam structure (just parameters)
GsnBm.x = 0 #Transverse Coordinates of Gaussian Beam Center at Waist [m]
GsnBm.y = 0
GsnBm.z = 0 #Longitudinal Coordinate of Waist [m]
GsnBm.xp = 0 #Average Angles of Gaussian Beam at Waist [rad]
GsnBm.yp = 0
GsnBm.avgPhotEn = 12400 #5000 #Photon Energy [eV]
GsnBm.pulseEn = 0.001 #Energy per Pulse [J] - to be corrected
GsnBm.repRate = 1 #Rep. Rate [Hz] - to be corrected
GsnBm.polar = 1 #1- linear hoirizontal
GsnBm.sigX = 23e-06/2.35 #Horiz. RMS size at Waist [m]
GsnBm.sigY = GsnBm.sigX #Vert. RMS size at Waist [m]
GsnBm.sigT = 10e-15 #Pulse duration [fs] (not used?)
GsnBm.mx = 0 #Transverse Gauss-Hermite Mode Orders
GsnBm.my = 0

wfr = SRWLWfr() #Initial Electric Field Wavefront
wfr.allocate(1, 100, 100) #Numbers of points vs Photon Energy (1), Horizontal and Vertical Positions (dummy)
wfr.mesh.zStart = 270 #Longitudinal Position [m] at which Electric Field has to be calculated, i.e. the position of the first optical element
wfr.mesh.eStart = GsnBm.avgPhotEn #Initial Photon Energy [eV]
wfr.mesh.eFin = GsnBm.avgPhotEn #Final Photon Energy [eV]
firstHorAp = 1.44e-03 #First Aperture [m]
firstVertAp = 1.448e-03 #[m] (dummy?)
wfr.mesh.xStart = -0.5*firstHorAp #Initial Horizontal Position [m]
wfr.mesh.xFin = 0.5*firstHorAp #Final Horizontal Position [m]
wfr.mesh.yStart = -0.5*firstVertAp #Initial Vertical Position [m]
wfr.mesh.yFin = 0.5*firstVertAp #Final Vertical Position [m]

sampFactNxNyForProp = 1.5 #sampling factor for adjusting nx, ny (effective if > 0)
arPrecPar = [sampFactNxNyForProp]

wfr.partBeam.partStatMom1.x = GsnBm.x #Some information about the source in the Wavefront structure
wfr.partBeam.partStatMom1.y = GsnBm.y
wfr.partBeam.partStatMom1.z = GsnBm.z
wfr.partBeam.partStatMom1.xp = GsnBm.xp
wfr.partBeam.partStatMom1.yp = GsnBm.yp

#**********************Calculating Initial Wavefront and extracting Intensity:
srwl.CalcElecFieldGaussian(wfr, GsnBm, arPrecPar)
mesh0 = deepcopy(wfr.mesh)
arI0 = array('f', [0]*mesh0.nx*mesh0.ny) #"flat" array to take 2D intensity data
srwl.CalcIntFromElecField(arI0, wfr, 6, 0, 3, mesh0.eStart, 0, 0) #extracts intensity
srwl_uti_save_intens_ascii(arI0, mesh0, os.path.join(os.getcwd(), strDataFolderName, 'ex09_res_int_in.dat'), 0)

arI0x = array('f', [0]*mesh0.nx) #array to take 1D intensity data (for plotting)
srwl.CalcIntFromElecField(arI0x, wfr, 6, 0, 1, mesh0.eStart, 0, 0) #extracts intensity
arI0y = array('f', [0]*mesh0.ny) #array to take 1D intensity data
srwl.CalcIntFromElecField(arI0y, wfr, 6, 0, 2, mesh0.eStart, 0, 0) #extracts intensity

arP0 = array('d', [0]*mesh0.nx*mesh0.ny) #"flat" array to take 2D phase data (note it should be 'd')
srwl.CalcIntFromElecField(arP0, wfr, 6, 4, 3, mesh0.eStart, 0, 0) #extracts radiation phase
srwl_uti_save_intens_ascii(arP0, mesh0, os.path.join(os.getcwd(), strDataFolderName, 'ex09_res_phase_in.dat'), 0, ['Photon Energy', 'Horizontal Position', 'Vertical Position', 'Phase'], _arUnits=['eV', 'm', 'm', 'rad'])

#**********************Optical Elements and Propagation related Input Parameters and Structures:

opDrM1_VFM = SRWLOptD(930 - 270 - 1.7) #Drift space from First Mirrors to KB

#read mirror slope arror data from file and setup the corresponding optical element
print('Defining Transmission element (to simulate mirror surface slope error)...', end='')
opTrErM1 = SRWLOptT(100, 1500, firstHorAp, firstVertAp)
heightProfData = AuxReadInDataColumns(os.path.join(os.getcwd(), strDataFolderName, "mirror1.dat"), 2, '\t')
AuxTransmAddSurfHeightProfile(opTrErM1, heightProfData, 'y', 1.8e-03)
print('done')
print('Saving optical path difference data to file (for viewing/debugging)...', end='')
opPathDifErM1 = opTrErM1.get_data(3, 3)
srwl_uti_save_intens_ascii(opPathDifErM1, opTrErM1.mesh, os.path.join(os.getcwd(), strDataFolderName, 'ex09_res_opt_path_dif_er_m1.dat'), 0, ['', 'Horizontal Position', 'Vertical Position', 'Opt. Path Diff.'], _arUnits=['', 'm', 'm', 'm'])
print('done')

opApKB = SRWLOptA('r', 'a', 1.98e-03, 1.98e-03) #Aperture of KB system

opLenVFM = SRWLOptL(1.e+20, 1.69689) #VFM simulated by Ideal Lens
print('Defining Transmission element (to simulate mirror surface slope error)...', end='')
opTrErVFM = SRWLOptT(100, 1500, 1.98e-03, 1.98e-03)
AuxTransmAddSurfHeightProfile(opTrErVFM, heightProfData, 'y', 3.6e-3)
print('done')
print('Saving optical path difference data to file (for viewing/debugging)...', end='')
opPathDifErVFM = opTrErVFM.get_data(3, 3)
srwl_uti_save_intens_ascii(opPathDifErVFM, opTrErVFM.mesh, os.path.join(os.getcwd(), strDataFolderName, 'ex09_res_opt_path_dif_er_vfm.dat'), 0, ['', 'Horizontal Position', 'Vertical Position', 'Opt. Path Diff.'], _arUnits=['', 'm', 'm', 'm'])
print('done')

opDrVFM_HFM = SRWLOptD(0.6) #Drift space between VFM and HFM

opLenHFM = SRWLOptL(1.0987, 1.e+20) #HFM simulated by Ideal Lens
print('Defining Transmission element (to simulate mirror surface slope error)...', end='')
opTrErHFM = SRWLOptT(1500, 100, 1.98e-03, 1.98e-03)
AuxTransmAddSurfHeightProfile(opTrErHFM, heightProfData, 'x', 3.6e-3)
print('done')
print('Saving optical path difference data to file (for viewing/debugging)...', end='')
opPathDifErHFM = opTrErHFM.get_data(3, 3)
srwl_uti_save_intens_ascii(opPathDifErHFM, opTrErHFM.mesh, os.path.join(os.getcwd(), strDataFolderName, 'ex09_res_opt_path_dif_er_hfm.dat'), 0, ['', 'Horizontal Position', 'Vertical Position', 'Opt. Path Diff.'], _arUnits=['', 'm', 'm', 'm'])
print('done')

opDrHFM_Samp = SRWLOptD(1.1) #Drift space from First Mirrors to KB

#Wavefront Propagation Parameters:
#[0]: Auto-Resize (1) or not (0) Before propagation
#[1]: Auto-Resize (1) or not (0) After propagation
#[2]: Relative Precision for propagation with Auto-Resizing (1. is nominal)
#[3]: Allow (1) or not (0) for semi-analytical treatment of the quadratic (leading) phase terms at the propagation
#[4]: Do any Resizing on Fourier side, using FFT, (1) or not (0)
#[5]: Horizontal Range modification factor at Resizing (1. means no modification)
#[6]: Horizontal Resolution modification factor at Resizing
#[7]: Vertical Range modification factor at Resizing
#[8]: Vertical Resolution modification factor at Resizing
#[9]: Type of wavefront Shift before Resizing (not yet implemented)
#[10]: New Horizontal wavefront Center position after Shift (not yet implemented)
#[11]: New Vertical wavefront Center position after Shift (not yet implemented)
prParRes1 = [0, 0, 1., 1, 0, 3., 1.0, 3., 1.0, 0, 0, 0]
prParRes2 = [0, 0, 1., 1, 0, 0.6, 8.0, 0.6, 4.0, 0, 0, 0]
prParRes0 = [0, 0, 1., 1, 0, 1.0, 1.0, 1.0, 1.0, 0, 0, 0]
prParRes3 = [0, 0, 1., 1, 0, 0.04, 5.0, 0.04, 5.0, 0, 0, 0]

#"Beamline" - Container of Optical Elements (together with the corresponding wavefront propagation instructions)
optBL = SRWLOptC([opTrErM1, opDrM1_VFM, opApKB, opLenVFM, opTrErVFM, opDrVFM_HFM, opLenHFM, opTrErHFM, opDrHFM_Samp],
                 [prParRes1, prParRes0, prParRes2, prParRes0, prParRes0, prParRes0, prParRes0, prParRes0, prParRes0, prParRes3])

#**********************Wavefront Propagation:
print('Propagating wavefront...', end='')
srwl.PropagElecField(wfr, optBL)
print('done')

print('Saving propagated wavefront intensity and phase to files...', end='')
mesh1 = deepcopy(wfr.mesh)
arI1 = array('f', [0]*mesh1.nx*mesh1.ny) #"flat" array to take 2D intensity data
srwl.CalcIntFromElecField(arI1, wfr, 6, 0, 3, mesh1.eStart, 0, 0) #extracts intensity
srwl_uti_save_intens_ascii(arI1, mesh1, os.path.join(os.getcwd(), strDataFolderName, 'ex09_res_int_prop.dat'), 0)

arI1x = array('f', [0]*mesh1.nx) #array to take 1D intensity data (for plotting)
srwl.CalcIntFromElecField(arI1x, wfr, 6, 0, 1, mesh1.eStart, 0, 0) #extracts intensity
arI1y = array('f', [0]*mesh1.ny) #array to take 1D intensity data
srwl.CalcIntFromElecField(arI1y, wfr, 6, 0, 2, mesh1.eStart, 0, 0) #extracts intensity

arP1 = array('d', [0]*mesh1.nx*mesh1.ny) #"flat" array to take 2D phase data (note it should be 'd')
srwl.CalcIntFromElecField(arP1, wfr, 6, 4, 3, mesh1.eStart, 0, 0) #extracts radiation phase
srwl_uti_save_intens_ascii(arP1, mesh1, os.path.join(os.getcwd(), strDataFolderName, 'ex09_res_phase_prop.dat'), 0, ['Photon Energy', 'Horizontal Position', 'Vertical Position', 'Phase'], _arUnits=['eV', 'm', 'm', 'rad'])
print('done')

#**********************Plotting results (requires 3rd party graphics package)
print('   Plotting the results (blocks script execution; close any graph windows to proceed) ... ', end='')
plotMesh0x = [1000*mesh0.xStart, 1000*mesh0.xFin, mesh0.nx]
plotMesh0y = [1000*mesh0.yStart, 1000*mesh0.yFin, mesh0.ny]
uti_plot2d(arI0, plotMesh0x, plotMesh0y, ['Horizontal Position [mm]', 'Vertical Position [mm]', 'Intensity Before Propagation'])
uti_plot1d(arI0x, plotMesh0x, ['Horizontal Position [mm]', 'Intensity [ph/s/.1%bw/mm^2]', 'Intensity (horizontal cut at y = 0)'])
uti_plot1d(arI0y, plotMesh0y, ['Vertical Position [mm]', 'Intensity [ph/s/.1%bw/mm^2]', 'Intensity (vertical cut at x = 0)'])
uti_plot2d(arP0, plotMesh0x, plotMesh0y, ['Horizontal Position [mm]', 'Vertical Position [mm]', 'Phase Before Propagation'])

plotMesh1x = [1e+06*mesh1.xStart, 1e+06*mesh1.xFin, mesh1.nx]
plotMesh1y = [1e+06*mesh1.yStart, 1e+06*mesh1.yFin, mesh1.ny]
uti_plot2d(arI1, plotMesh1x, plotMesh1y, ['Horizontal Position [um]', 'Vertical Position [um]', 'Intensity After Propagation'])
uti_plot1d(arI1x, plotMesh1x, ['Horizontal Position [um]', 'Intensity [ph/s/.1%bw/mm^2]', 'Intensity (horizontal cut at y = 0)'])
uti_plot1d(arI1y, plotMesh1y, ['Vertical Position [um]', 'Intensity [ph/s/.1%bw/mm^2]', 'Intensity (vertical cut at x = 0)'])
uti_plot2d(arP1, plotMesh1x, plotMesh1y, ['Horizontal Position [um]', 'Vertical Position [um]', 'Phase After Propagation'])

uti_plot_show() #show all graphs (blocks script execution; close all graph windows to proceed)
print('done')
