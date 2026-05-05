import React, { useEffect, useState } from 'react';
import {
  ActivityIndicator,
  FlatList,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from 'react-native';
import { useRouter } from 'expo-router';

import {
  discoverDevices,
  DiscoveredDevice,
  ScanProgress,
} from '../src/api/discover';
import { DeviceStorage } from '../src/utils/storage';

export default function DiscoverScreen() {
  const router = useRouter();
  const [scanning, setScanning] = useState(false);
  const [progress, setProgress] = useState<ScanProgress>({ probed: 0, total: 0 });
  const [devices, setDevices] = useState<DiscoveredDevice[]>([]);
  const [error, setError] = useState<string | null>(null);

  const startScan = async () => {
    setScanning(true);
    setError(null);
    setDevices([]);
    setProgress({ probed: 0, total: 0 });
    try {
      const found = await discoverDevices(setProgress);
      setDevices(found);
    } catch (e: any) {
      setError(e?.message ?? 'Scan failed.');
    } finally {
      setScanning(false);
    }
  };

  useEffect(() => {
    void startScan();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const select = async (device: DiscoveredDevice) => {
    // HTTP-scanned devices have no auth token. setIPAndToken parses the URL
    // and only stores a token if the URL has ?t=... — which it won't here.
    await DeviceStorage.setIPAndToken(device.ip);
    router.back();
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Discover Devices</Text>
      <Text style={styles.subtitle}>
        Scanning the local Wi-Fi for Crosspoint Readers.
      </Text>

      {scanning && (
        <View style={styles.statusBox}>
          <ActivityIndicator color="#007AFF" />
          <Text style={styles.statusText}>
            {progress.total > 0
              ? `Scanning ${progress.probed}/${progress.total}`
              : 'Initializing…'}
          </Text>
        </View>
      )}

      {!!error && <Text style={styles.errorText}>{error}</Text>}

      <FlatList
        style={styles.list}
        data={devices}
        keyExtractor={(d) => d.ip}
        ItemSeparatorComponent={() => <View style={styles.sep} />}
        renderItem={({ item }) => (
          <TouchableOpacity style={styles.row} onPress={() => select(item)}>
            <View style={{ flex: 1 }}>
              <Text style={styles.rowTitle}>{item.ip}</Text>
              <Text style={styles.rowSubtitle}>
                {item.needsAuth
                  ? 'Auth required — pair with QR for full access'
                  : 'Open / no token required'}
                {'  ·  '}
                {item.reachableMs}ms
              </Text>
            </View>
            <Text style={styles.chev}>›</Text>
          </TouchableOpacity>
        )}
        ListEmptyComponent={
          !scanning ? (
            <View style={styles.emptyBox}>
              <Text style={styles.emptyText}>
                {error ? '' : 'No Crosspoint Readers on this Wi-Fi.'}
              </Text>
              <Text style={styles.emptyHint}>
                Make sure your phone and reader are on the same network.
              </Text>
              <Text style={styles.emptyHint}>
                Tip: this scan only covers the local /24 subnet (×.×.×.1–254).
                If your network uses /16 or a separate guest VLAN, the device
                won&apos;t be discovered — pair via QR code instead.
              </Text>
            </View>
          ) : null
        }
      />

      <View style={styles.actions}>
        <TouchableOpacity
          style={[styles.button, scanning && styles.buttonDisabled]}
          onPress={startScan}
          disabled={scanning}
        >
          <Text style={styles.buttonText}>
            {scanning ? 'Scanning…' : 'Scan Again'}
          </Text>
        </TouchableOpacity>

        <TouchableOpacity
          style={styles.linkButton}
          onPress={() => router.replace('/scanner')}
        >
          <Text style={styles.linkButtonText}>Use QR code instead</Text>
        </TouchableOpacity>

        <TouchableOpacity style={styles.linkButton} onPress={() => router.back()}>
          <Text style={styles.linkButtonText}>Cancel</Text>
        </TouchableOpacity>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, paddingTop: 60, backgroundColor: '#fff' },
  title: { fontSize: 28, fontWeight: 'bold', marginBottom: 6 },
  subtitle: { fontSize: 14, color: '#666', marginBottom: 16 },
  statusBox: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 12,
    marginBottom: 16,
    padding: 12,
    backgroundColor: '#f0f4f8',
    borderRadius: 8,
  },
  statusText: { color: '#333', fontSize: 14 },
  errorText: { color: '#c44', fontSize: 14, marginBottom: 16 },
  list: { flex: 1 },
  row: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 12,
    paddingHorizontal: 4,
  },
  rowTitle: { fontSize: 16, fontWeight: '600', color: '#333' },
  rowSubtitle: { fontSize: 12, color: '#888', marginTop: 2 },
  chev: { fontSize: 24, color: '#bbb', paddingLeft: 8 },
  sep: { height: StyleSheet.hairlineWidth, backgroundColor: '#eee' },
  emptyBox: { padding: 20, alignItems: 'center' },
  emptyText: { color: '#666', fontSize: 14, marginBottom: 6 },
  emptyHint: { color: '#999', fontSize: 12, textAlign: 'center' },
  actions: { marginTop: 20 },
  button: {
    backgroundColor: '#007AFF',
    padding: 14,
    borderRadius: 10,
    alignItems: 'center',
  },
  buttonDisabled: { backgroundColor: '#A0A0A0' },
  buttonText: { color: '#fff', fontWeight: '600', fontSize: 16 },
  linkButton: { padding: 12, alignItems: 'center' },
  linkButtonText: { color: '#007AFF', fontSize: 14 },
});
