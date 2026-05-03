import React, { useState, useEffect } from 'react';
import { StyleSheet, Text, View, Button, TouchableOpacity } from 'react-native';
import { CameraView, useCameraPermissions, BarcodeScanningResult } from 'expo-camera';
import { useRouter } from 'expo-router';
import { DeviceStorage } from '../src/utils/storage';
import { DeviceApi } from '../src/api/device';

export default function ScannerScreen() {
  const [permission, requestPermission] = useCameraPermissions();
  const [scanned, setScanned] = useState(false);
  const router = useRouter();

  if (!permission) {
    // Camera permissions are still loading.
    return <View />;
  }

  if (!permission.granted) {
    // Camera permissions are not granted yet.
    return (
      <View style={styles.container}>
        <Text style={styles.message}>We need your permission to show the camera</Text>
        <Button onPress={requestPermission} title="Grant Permission" />
      </View>
    );
  }

  const handleBarCodeScanned = async (result: BarcodeScanningResult) => {
    if (scanned) return;
    setScanned(true);

    const data = result.data;
    console.log('Scanned QR:', data);

    // Validate if it's a URL or IP
    if (data.startsWith('http://') || data.startsWith('https://')) {
      try {
        await DeviceStorage.setIP(data);
        const isResponsive = await DeviceApi.ping();
        if (isResponsive) {
          alert('Connected to Crosspoint Reader!');
          router.back();
        } else {
          alert('Device found but not responding. Check connection.');
          setScanned(false);
        }
      } catch (e) {
        alert('Failed to save device IP');
        setScanned(false);
      }
    } else {
      alert('Invalid QR Code. Please scan the connection QR on your device.');
      setScanned(false);
    }
  };

  return (
    <View style={styles.container}>
      <CameraView
        style={styles.camera}
        onBarcodeScanned={scanned ? undefined : handleBarCodeScanned}
        barcodeScannerSettings={{
          barcodeTypes: ['qr'],
        }}
      >
        <View style={styles.overlay}>
          <View style={styles.unfocusedContainer} />
          <View style={styles.focusedContainer}>
            <View style={styles.unfocusedContainer} />
            <View style={styles.focusedView} />
            <View style={styles.unfocusedContainer} />
          </View>
          <View style={styles.unfocusedContainer} />
        </View>
      </CameraView>
      
      <TouchableOpacity style={styles.closeButton} onPress={() => router.back()}>
        <Text style={styles.closeButtonText}>Cancel</Text>
      </TouchableOpacity>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    flexDirection: 'column',
    justifyContent: 'center',
    backgroundColor: '#000',
  },
  message: {
    textAlign: 'center',
    paddingBottom: 10,
    color: 'white',
  },
  camera: {
    flex: 1,
  },
  overlay: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
  },
  unfocusedContainer: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.5)',
  },
  focusedContainer: {
    flex: 2,
    flexDirection: 'row',
  },
  focusedView: {
    flex: 6,
    borderColor: '#fff',
    borderWidth: 2,
    borderRadius: 10,
  },
  closeButton: {
    position: 'absolute',
    bottom: 40,
    alignSelf: 'center',
    padding: 15,
    backgroundColor: 'rgba(255,255,255,0.3)',
    borderRadius: 10,
  },
  closeButtonText: {
    color: '#fff',
    fontSize: 18,
  },
});
