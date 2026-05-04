import React, { useCallback, useEffect, useState } from 'react';
import {
  ActivityIndicator,
  Alert,
  FlatList,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from 'react-native';
import { useRouter } from 'expo-router';

import { FontApi, FontPack } from '../src/api/fonts';

export default function FontsScreen() {
  const router = useRouter();
  const [packs, setPacks] = useState<FontPack[] | null>(null);
  const [loading, setLoading] = useState(true);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const fetchPacks = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      setPacks(await FontApi.list());
    } catch (e: any) {
      setError(e?.message ?? 'Failed to load fonts.');
      setPacks(null);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void fetchPacks();
  }, [fetchPacks]);

  const reload = async () => {
    setBusy(true);
    try {
      await FontApi.reload();
      await fetchPacks();
      Alert.alert('Reloaded', 'Font packs rescanned on device.');
    } catch (e: any) {
      Alert.alert('Reload failed', e?.message ?? 'Could not reload fonts.');
    } finally {
      setBusy(false);
    }
  };

  const confirmDelete = (pack: FontPack) => {
    Alert.alert(
      'Delete font pack',
      `Permanently delete "${pack.name}" from the device's SD card?`,
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Delete',
          style: 'destructive',
          onPress: async () => {
            setBusy(true);
            try {
              await FontApi.remove(pack.path);
              await fetchPacks();
            } catch (e: any) {
              Alert.alert(
                'Delete failed',
                e?.message ?? 'Could not delete font pack.',
              );
            } finally {
              setBusy(false);
            }
          },
        },
      ],
    );
  };

  if (loading) {
    return (
      <View style={styles.centerContainer}>
        <ActivityIndicator size="large" color="#007AFF" />
        <Text style={styles.helperText}>Fetching font packs…</Text>
      </View>
    );
  }

  if (error || !packs) {
    return (
      <View style={styles.centerContainer}>
        <Text style={styles.errorText}>{error ?? 'Failed to load fonts.'}</Text>
        <TouchableOpacity style={styles.primaryButton} onPress={fetchPacks}>
          <Text style={styles.primaryButtonText}>Retry</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.linkButton} onPress={() => router.back()}>
          <Text style={styles.linkButtonText}>Cancel</Text>
        </TouchableOpacity>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Font Packs</Text>
      <Text style={styles.subtitle}>
        Multilingual font packs installed on the device.
      </Text>

      <FlatList
        style={styles.list}
        data={packs}
        keyExtractor={(p) => p.path}
        ItemSeparatorComponent={() => <View style={styles.sep} />}
        renderItem={({ item }) => (
          <View style={styles.row}>
            <View style={styles.rowMain}>
              <Text style={styles.rowTitle}>{item.name}</Text>
              <Text style={styles.rowPath} numberOfLines={1}>
                {item.path}
              </Text>
              <View style={styles.badgeRow}>
                {item.active ? (
                  <View style={[styles.badge, styles.badgeActive]}>
                    <Text style={styles.badgeText}>Active</Text>
                  </View>
                ) : item.installed ? (
                  <View style={[styles.badge, styles.badgeInstalled]}>
                    <Text style={styles.badgeText}>Installed</Text>
                  </View>
                ) : (
                  <View style={[styles.badge, styles.badgeMissing]}>
                    <Text style={styles.badgeText}>Missing</Text>
                  </View>
                )}
              </View>
            </View>
            <TouchableOpacity
              style={[styles.deleteButton, busy && styles.buttonDisabled]}
              onPress={() => confirmDelete(item)}
              disabled={busy}
            >
              <Text style={styles.deleteButtonText}>Delete</Text>
            </TouchableOpacity>
          </View>
        )}
        ListEmptyComponent={
          <View style={styles.emptyBox}>
            <Text style={styles.emptyText}>No font packs installed.</Text>
            <Text style={styles.emptyHint}>
              Drop .epf packs under /.mofei/fonts/ on the SD card and tap Reload.
            </Text>
          </View>
        }
      />

      <View style={styles.actions}>
        <TouchableOpacity
          style={[styles.button, busy && styles.buttonDisabled]}
          onPress={reload}
          disabled={busy}
        >
          {busy ? (
            <ActivityIndicator color="#fff" />
          ) : (
            <Text style={styles.buttonText}>Reload Font Packs</Text>
          )}
        </TouchableOpacity>
        <TouchableOpacity style={styles.linkButton} onPress={() => router.back()}>
          <Text style={styles.linkButtonText}>Done</Text>
        </TouchableOpacity>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, paddingTop: 60, backgroundColor: '#fff' },
  centerContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#fff',
    padding: 20,
  },
  title: { fontSize: 28, fontWeight: 'bold', marginBottom: 6 },
  subtitle: { fontSize: 14, color: '#666', marginBottom: 16 },
  helperText: { marginTop: 10, fontSize: 16, color: '#666' },
  errorText: { fontSize: 16, color: '#c44', textAlign: 'center', marginBottom: 20 },
  list: { flex: 1 },
  row: { flexDirection: 'row', alignItems: 'center', paddingVertical: 12 },
  rowMain: { flex: 1, marginRight: 12 },
  rowTitle: { fontSize: 16, fontWeight: '600', color: '#333' },
  rowPath: { fontSize: 12, color: '#888', marginTop: 2 },
  badgeRow: { flexDirection: 'row', gap: 6, marginTop: 6 },
  badge: { paddingVertical: 2, paddingHorizontal: 8, borderRadius: 10 },
  badgeActive: { backgroundColor: '#34C759' },
  badgeInstalled: { backgroundColor: '#A0A0A0' },
  badgeMissing: { backgroundColor: '#FF3B30' },
  badgeText: { color: '#fff', fontSize: 11, fontWeight: '600' },
  deleteButton: {
    paddingVertical: 8,
    paddingHorizontal: 12,
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#FF3B30',
  },
  deleteButtonText: { color: '#FF3B30', fontWeight: '600', fontSize: 13 },
  sep: { height: StyleSheet.hairlineWidth, backgroundColor: '#eee' },
  emptyBox: { padding: 20, alignItems: 'center' },
  emptyText: { color: '#666', fontSize: 14, marginBottom: 6 },
  emptyHint: { color: '#999', fontSize: 12, textAlign: 'center' },
  actions: { marginTop: 16 },
  button: {
    backgroundColor: '#007AFF',
    padding: 14,
    borderRadius: 10,
    alignItems: 'center',
  },
  buttonDisabled: { opacity: 0.6, backgroundColor: '#A0A0A0' },
  buttonText: { color: '#fff', fontWeight: '600', fontSize: 16 },
  primaryButton: {
    backgroundColor: '#007AFF',
    paddingVertical: 12,
    paddingHorizontal: 30,
    borderRadius: 10,
    marginTop: 10,
  },
  primaryButtonText: { color: '#fff', fontSize: 16, fontWeight: '600' },
  linkButton: { padding: 12, alignItems: 'center' },
  linkButtonText: { color: '#007AFF', fontSize: 14 },
});
