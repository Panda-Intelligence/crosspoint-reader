import { useEffect, useMemo, useState } from 'react';
import {
  ActivityIndicator,
  Alert,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from 'react-native';
import { useRouter } from 'expo-router';
import { useIsFocused } from '@react-navigation/native';

import ParallaxScrollView from '@/components/parallax-scroll-view';
import { ThemedText } from '@/components/themed-text';
import { ThemedView } from '@/components/themed-view';
import { IconSymbol } from '@/components/ui/icon-symbol';
import { DeviceApi, DeviceSetting } from '../../src/api/device';
import { DeviceStatus, StatusApi } from '../../src/api/status';
import { DeviceStorage } from '../../src/utils/storage';

type ConnStatus = 'idle' | 'loading' | 'reachable' | 'unreachable' | 'error';

export default function DeviceScreen() {
  const router = useRouter();
  const isFocused = useIsFocused();

  const [ip, setIp] = useState<string | null>(null);
  const [status, setStatus] = useState<ConnStatus>('idle');
  const [settings, setSettings] = useState<DeviceSetting[] | null>(null);
  const [deviceStatus, setDeviceStatus] = useState<DeviceStatus | null>(null);
  const [refreshing, setRefreshing] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const refresh = async () => {
    setRefreshing(true);
    setError(null);
    try {
      const storedIp = await DeviceStorage.getIP();
      setIp(storedIp);
      if (!storedIp) {
        setStatus('idle');
        setSettings(null);
        setDeviceStatus(null);
        return;
      }
      setStatus('loading');
      const reachable = await DeviceApi.ping();
      setStatus(reachable ? 'reachable' : 'unreachable');
      if (reachable) {
        // Fetch status + settings in parallel.
        const [statusResult, settingsResult] = await Promise.allSettled([
          StatusApi.get(),
          DeviceApi.getSettings(),
        ]);
        if (statusResult.status === 'fulfilled') {
          setDeviceStatus(statusResult.value);
        } else {
          setDeviceStatus(null);
        }
        if (settingsResult.status === 'fulfilled') {
          setSettings(settingsResult.value);
        } else {
          setSettings(null);
          setError(
            settingsResult.reason?.message ?? 'Could not fetch settings.',
          );
        }
      } else {
        setSettings(null);
        setDeviceStatus(null);
      }
    } catch (e: any) {
      setStatus('error');
      setError(e?.message ?? 'Failed to refresh.');
    } finally {
      setRefreshing(false);
    }
  };

  useEffect(() => {
    if (isFocused) {
      void refresh();
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [isFocused]);

  const summary = useMemo(() => {
    if (!settings) return [] as Array<[string, DeviceSetting[]]>;
    const groups: Record<string, DeviceSetting[]> = {};
    for (const s of settings) {
      if (!groups[s.category]) groups[s.category] = [];
      groups[s.category].push(s);
    }
    return Object.entries(groups);
  }, [settings]);

  const handleUnbind = () => {
    Alert.alert(
      'Disconnect Device',
      'Are you sure you want to unbind from the current reader?',
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Unbind',
          style: 'destructive',
          onPress: async () => {
            await DeviceStorage.clearDevice();
            await refresh();
          },
        },
      ],
    );
  };

  return (
    <ParallaxScrollView
      headerBackgroundColor={{ light: '#D0D0D0', dark: '#353636' }}
      headerImage={
        <IconSymbol
          size={310}
          color="#808080"
          name="gearshape.fill"
          style={styles.headerImage}
        />
      }>
      <ThemedView style={styles.titleContainer}>
        <ThemedText type="title">Device</ThemedText>
      </ThemedView>

      {/* Connection */}
      <ThemedView style={styles.section}>
        <ThemedText type="subtitle">Connection</ThemedText>
        {ip ? (
          <View style={styles.statusBox}>
            <View style={[styles.statusDot, statusDotStyle(status)]} />
            <View style={styles.statusTextWrap}>
              <Text style={styles.statusIp}>{ip}</Text>
              <Text style={styles.statusLabel}>{statusLabel(status)}</Text>
            </View>
            <TouchableOpacity
              style={styles.linkAction}
              onPress={refresh}
              disabled={refreshing}>
              {refreshing ? (
                <ActivityIndicator color="#007AFF" size="small" />
              ) : (
                <Text style={styles.linkActionText}>Refresh</Text>
              )}
            </TouchableOpacity>
          </View>
        ) : (
          <>
            <ThemedText>No device paired yet.</ThemedText>
            <TouchableOpacity
              style={styles.connectButton}
              onPress={() => router.push('/scanner')}>
              <Text style={styles.buttonText}>Scan QR Code</Text>
            </TouchableOpacity>
          </>
        )}
      </ThemedView>

      {/* Live device status */}
      {ip && status === 'reachable' && deviceStatus && (
        <ThemedView style={styles.section}>
          <ThemedText type="subtitle">Status</ThemedText>
          <View style={styles.statusGrid}>
            <StatusCell label="Firmware" value={deviceStatus.version} />
            <StatusCell label="Mode" value={deviceStatus.mode} />
            <StatusCell
              label="RSSI"
              value={
                deviceStatus.mode === 'AP' || deviceStatus.rssi === 0
                  ? '—'
                  : `${deviceStatus.rssi} dBm`
              }
            />
            <StatusCell
              label="Free heap"
              value={`${(deviceStatus.freeHeap / 1024).toFixed(0)} KB`}
            />
            <StatusCell
              label="Uptime"
              value={formatUptime(deviceStatus.uptime)}
            />
          </View>
        </ThemedView>
      )}

      {/* Settings summary (only when reachable) */}
      {ip && status === 'reachable' && (
        <ThemedView style={styles.section}>
          <ThemedText type="subtitle">Current Settings</ThemedText>
          {settings ? (
            summary.length === 0 ? (
              <ThemedText>(empty)</ThemedText>
            ) : (
              summary.map(([category, rows]) => (
                <View key={category} style={styles.summaryGroup}>
                  <Text style={styles.summaryCategory}>{category}</Text>
                  {rows.map((s) => (
                    <TouchableOpacity
                      key={s.key}
                      style={styles.summaryRow}
                      onPress={() => router.push('/settings')}>
                      <Text style={styles.summaryName} numberOfLines={1}>
                        {s.name}
                      </Text>
                      <Text style={styles.summaryValue} numberOfLines={1}>
                        {formatValue(s)}
                      </Text>
                      <IconSymbol name="chevron.right" size={16} color="#888" />
                    </TouchableOpacity>
                  ))}
                </View>
              ))
            )
          ) : (
            <ThemedText>{error ?? 'Loading settings…'}</ThemedText>
          )}
          <TouchableOpacity
            style={styles.editButton}
            onPress={() => router.push('/settings')}>
            <Text style={styles.buttonText}>Edit Settings</Text>
          </TouchableOpacity>
        </ThemedView>
      )}

      {/* Library — fonts, OPDS catalogs, files (only when reachable) */}
      {ip && status === 'reachable' && (
        <ThemedView style={styles.section}>
          <ThemedText type="subtitle">Library</ThemedText>
          <TouchableOpacity
            style={styles.summaryRow}
            onPress={() => router.push('/files')}>
            <Text style={styles.summaryName}>Files</Text>
            <Text style={styles.summaryValue}>Browse SD</Text>
            <IconSymbol name="chevron.right" size={16} color="#888" />
          </TouchableOpacity>
          <TouchableOpacity
            style={styles.summaryRow}
            onPress={() => router.push('/fonts')}>
            <Text style={styles.summaryName}>Font Packs</Text>
            <Text style={styles.summaryValue}>Manage</Text>
            <IconSymbol name="chevron.right" size={16} color="#888" />
          </TouchableOpacity>
          <TouchableOpacity
            style={styles.summaryRow}
            onPress={() => router.push('/opds')}>
            <Text style={styles.summaryName}>OPDS Catalogs</Text>
            <Text style={styles.summaryValue}>Manage</Text>
            <IconSymbol name="chevron.right" size={16} color="#888" />
          </TouchableOpacity>
        </ThemedView>
      )}

      {/* Unbind */}
      {ip && (
        <ThemedView style={styles.section}>
          <TouchableOpacity style={styles.unbindButton} onPress={handleUnbind}>
            <Text style={styles.unbindButtonText}>Unbind Device</Text>
          </TouchableOpacity>
        </ThemedView>
      )}
    </ParallaxScrollView>
  );
}

function statusLabel(status: ConnStatus): string {
  switch (status) {
    case 'idle': return '—';
    case 'loading': return 'Checking…';
    case 'reachable': return 'Reachable';
    case 'unreachable': return 'Unreachable';
    case 'error': return 'Error';
  }
}

function statusDotStyle(status: ConnStatus) {
  switch (status) {
    case 'reachable':   return { backgroundColor: '#34C759' };
    case 'unreachable': return { backgroundColor: '#FF3B30' };
    case 'loading':     return { backgroundColor: '#FF9500' };
    default:            return { backgroundColor: '#A0A0A0' };
  }
}

function formatValue(s: DeviceSetting): string {
  switch (s.type) {
    case 'toggle': return s.value ? 'On' : 'Off';
    case 'enum':   return s.options[s.value] ?? `#${s.value}`;
    case 'value':  return String(s.value);
    case 'string': return s.value || '(empty)';
  }
}

function formatUptime(seconds: number): string {
  if (seconds < 60) return `${seconds}s`;
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = seconds % 60;
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m ${s}s`;
}

function StatusCell({ label, value }: { label: string; value: string }) {
  return (
    <View style={styles.statusCell}>
      <Text style={styles.statusCellValue} numberOfLines={1}>
        {value}
      </Text>
      <Text style={styles.statusCellLabel}>{label}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  headerImage: {
    color: '#808080',
    bottom: -90,
    left: -35,
    position: 'absolute',
  },
  titleContainer: { flexDirection: 'row', alignItems: 'center', gap: 8 },
  section: { gap: 8, marginBottom: 16 },
  statusBox: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#f0f4f8',
    padding: 12,
    borderRadius: 8,
    gap: 12,
  },
  statusDot: { width: 12, height: 12, borderRadius: 6 },
  statusTextWrap: { flex: 1 },
  statusIp: { fontSize: 15, fontWeight: '600', color: '#333' },
  statusLabel: { fontSize: 12, color: '#666', marginTop: 2 },
  linkAction: { paddingHorizontal: 8, paddingVertical: 4 },
  linkActionText: { color: '#007AFF', fontWeight: '600' },
  connectButton: {
    backgroundColor: '#007AFF',
    padding: 12,
    borderRadius: 8,
    alignItems: 'center',
    marginTop: 8,
  },
  editButton: {
    backgroundColor: '#FF9500',
    padding: 12,
    borderRadius: 8,
    alignItems: 'center',
    marginTop: 12,
  },
  buttonText: { color: '#fff', fontWeight: '600', fontSize: 16 },
  summaryGroup: { marginTop: 8 },
  summaryCategory: {
    fontSize: 12,
    fontWeight: '600',
    color: '#888',
    textTransform: 'uppercase',
    letterSpacing: 0.5,
    marginBottom: 4,
  },
  summaryRow: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 8,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#eee',
    gap: 8,
  },
  summaryName: { flex: 1, fontSize: 14, color: '#333' },
  summaryValue: { fontSize: 14, color: '#888', maxWidth: 140 },
  unbindButton: {
    borderWidth: 1,
    borderColor: '#FF3B30',
    padding: 12,
    borderRadius: 8,
    alignItems: 'center',
    marginTop: 16,
  },
  unbindButtonText: { color: '#FF3B30', fontWeight: '600', fontSize: 16 },
  statusGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 8,
    marginTop: 4,
  },
  statusCell: {
    flexBasis: '31%',
    flexGrow: 1,
    backgroundColor: '#f0f4f8',
    paddingVertical: 10,
    paddingHorizontal: 8,
    borderRadius: 8,
    minWidth: 90,
  },
  statusCellValue: { fontSize: 15, fontWeight: '600', color: '#333' },
  statusCellLabel: {
    fontSize: 11,
    color: '#888',
    marginTop: 2,
    textTransform: 'uppercase',
    letterSpacing: 0.4,
  },
});
