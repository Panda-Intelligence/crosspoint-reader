import React, { useCallback, useEffect, useState } from 'react';
import {
  ActivityIndicator,
  Alert,
  FlatList,
  Modal,
  StyleSheet,
  Text,
  TextInput,
  TouchableOpacity,
  View,
} from 'react-native';
import * as DocumentPicker from 'expo-document-picker';
import { useRouter } from 'expo-router';

import { TransferApi } from '../src/api/transfer';
import { DeviceFileEntry, FilesApi } from '../src/api/files';

export default function TransferScreen() {
  const router = useRouter();
  const [uploading, setUploading] = useState(false);
  const [progress, setProgress] = useState(0);
  const [target, setTarget] = useState<string>('/');
  const [pickerOpen, setPickerOpen] = useState(false);

  const pickAndUploadFile = async () => {
    try {
      const result = await DocumentPicker.getDocumentAsync({
        type: ['application/epub+zip', 'text/plain', 'application/zip'],
        copyToCacheDirectory: true,
      });

      if (result.canceled || !result.assets || result.assets.length === 0) {
        return;
      }

      const file = result.assets[0];

      setUploading(true);
      setProgress(0);

      await TransferApi.uploadFile(
        {
          uri: file.uri,
          name: file.name,
          type: file.mimeType || 'application/octet-stream',
        },
        {
          targetPath: target === '/' ? undefined : target,
          onProgress: (p) => setProgress(p),
        },
      );

      setUploading(false);
      Alert.alert(
        'Success',
        `${file.name} sent to ${target === '/' ? 'the SD root' : target}.`,
      );
    } catch (error: any) {
      setUploading(false);
      Alert.alert('Upload Failed', error.message || 'Check your connection to the Reader.');
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>File Transfer</Text>
      <Text style={styles.subtitle}>Send EPUB, TXT, or CBZ files to your device</Text>

      <View style={styles.destBox}>
        <View style={styles.destInfo}>
          <Text style={styles.destLabel}>Destination</Text>
          <Text style={styles.destPath} numberOfLines={1} ellipsizeMode="middle">
            {target}
          </Text>
        </View>
        <TouchableOpacity
          style={styles.destButton}
          onPress={() => setPickerOpen(true)}
          disabled={uploading}>
          <Text style={styles.destButtonText}>Choose folder…</Text>
        </TouchableOpacity>
      </View>

      <TouchableOpacity
        style={[styles.button, uploading && styles.buttonDisabled]}
        onPress={pickAndUploadFile}
        disabled={uploading}>
        {uploading ? (
          <ActivityIndicator color="#fff" />
        ) : (
          <Text style={styles.buttonText}>Pick & Send File</Text>
        )}
      </TouchableOpacity>

      {uploading && (
        <View style={styles.progressContainer}>
          <Text style={styles.progressText}>Uploading: {progress}%</Text>
          <View style={styles.progressBar}>
            <View style={[styles.progressFill, { width: `${progress}%` }]} />
          </View>
        </View>
      )}

      <TouchableOpacity style={styles.backButton} onPress={() => router.back()}>
        <Text style={styles.backButtonText}>Go Back</Text>
      </TouchableOpacity>

      <FolderPickerModal
        visible={pickerOpen}
        initialPath={target}
        onCancel={() => setPickerOpen(false)}
        onConfirm={(path) => {
          setTarget(path);
          setPickerOpen(false);
        }}
      />
    </View>
  );
}

interface FolderPickerProps {
  visible: boolean;
  initialPath: string;
  onCancel: () => void;
  onConfirm: (path: string) => void;
}

// Inline folder picker — kept local rather than extracted to a shared
// component. Lists only directories, supports navigating up. Includes
// a "+ New folder" affordance so users can create a destination
// without leaving the upload flow.
function FolderPickerModal({ visible, initialPath, onCancel, onConfirm }: FolderPickerProps) {
  const [path, setPath] = useState(initialPath);
  const [entries, setEntries] = useState<DeviceFileEntry[] | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // mkdir sub-modal state.
  const [mkdirOpen, setMkdirOpen] = useState(false);
  const [mkdirName, setMkdirName] = useState('');
  const [mkdirBusy, setMkdirBusy] = useState(false);

  const fetchList = useCallback(async (p: string) => {
    setLoading(true);
    setError(null);
    try {
      const list = await FilesApi.list(p);
      setEntries(list.filter((e) => e.isDirectory));
    } catch (e: any) {
      setError(e?.message ?? 'Failed to load.');
      setEntries(null);
    } finally {
      setLoading(false);
    }
  }, []);

  // Reset to initialPath each time the modal becomes visible.
  useEffect(() => {
    if (visible) {
      setPath(initialPath);
    }
  }, [visible, initialPath]);

  useEffect(() => {
    if (visible) void fetchList(path);
  }, [visible, path, fetchList]);

  const submitMkdir = async () => {
    const name = mkdirName.trim();
    if (!name) return;
    if (name.includes('/') || name.includes('\\')) {
      Alert.alert('Invalid name', 'Folder name cannot contain slashes.');
      return;
    }
    setMkdirBusy(true);
    try {
      await FilesApi.mkdir(path, name);
      setMkdirOpen(false);
      setMkdirName('');
      await fetchList(path);
    } catch (e: any) {
      Alert.alert('Create folder failed', e?.message ?? 'Could not create folder.');
    } finally {
      setMkdirBusy(false);
    }
  };

  return (
    <Modal animationType="slide" transparent={false} visible={visible} onRequestClose={onCancel}>
      <View style={pickerStyles.container}>
        <View style={pickerStyles.pathBar}>
          <TouchableOpacity
            onPress={() => (path === '/' ? onCancel() : setPath(FilesApi.parentOf(path)))}
            style={pickerStyles.upButton}
            accessibilityRole="button"
            accessibilityLabel={path === '/' ? 'Cancel' : 'Up one folder'}>
            <Text style={pickerStyles.upButtonText}>{path === '/' ? '‹ Cancel' : '‹ Up'}</Text>
          </TouchableOpacity>
          <Text style={pickerStyles.pathText} numberOfLines={1} ellipsizeMode="middle">
            {path}
          </Text>
          <TouchableOpacity
            onPress={() => setMkdirOpen(true)}
            style={pickerStyles.mkdirButton}
            accessibilityRole="button"
            accessibilityLabel="Create new folder here">
            <Text style={pickerStyles.mkdirButtonText}>+ Folder</Text>
          </TouchableOpacity>
        </View>

        {loading ? (
          <View style={pickerStyles.center}>
            <ActivityIndicator size="large" color="#007AFF" />
          </View>
        ) : error ? (
          <View style={pickerStyles.center}>
            <Text style={pickerStyles.errorText}>{error}</Text>
            <TouchableOpacity style={pickerStyles.retry} onPress={() => fetchList(path)}>
              <Text style={pickerStyles.retryText}>Retry</Text>
            </TouchableOpacity>
          </View>
        ) : (
          <FlatList
            data={entries ?? []}
            keyExtractor={(e) => `d:${e.name}`}
            ItemSeparatorComponent={() => <View style={pickerStyles.sep} />}
            renderItem={({ item }) => (
              <TouchableOpacity
                style={pickerStyles.row}
                onPress={() => setPath(FilesApi.joinPath(path, item.name))}
                accessibilityRole="button"
                accessibilityLabel={`Enter folder ${item.name}`}>
                <Text style={pickerStyles.icon}>📁</Text>
                <Text style={pickerStyles.rowName} numberOfLines={1}>
                  {item.name}
                </Text>
                <Text style={pickerStyles.chev}>›</Text>
              </TouchableOpacity>
            )}
            ListEmptyComponent={
              <View style={pickerStyles.center}>
                <Text style={pickerStyles.emptyText}>(no sub-folders)</Text>
              </View>
            }
          />
        )}

        <View style={pickerStyles.actions}>
          <TouchableOpacity
            style={pickerStyles.confirm}
            onPress={() => onConfirm(path)}
            accessibilityRole="button"
            accessibilityLabel={`Use folder ${path}`}>
            <Text style={pickerStyles.confirmText}>Use this folder ({path})</Text>
          </TouchableOpacity>
        </View>

        <Modal
          animationType="fade"
          transparent
          visible={mkdirOpen}
          onRequestClose={() => setMkdirOpen(false)}>
          <View style={pickerStyles.modalBackdrop}>
            <View style={pickerStyles.modalCard}>
              <Text style={pickerStyles.modalTitle}>New Folder</Text>
              <Text style={pickerStyles.modalHint}>under {path}</Text>
              <TextInput
                style={pickerStyles.modalInput}
                value={mkdirName}
                onChangeText={setMkdirName}
                placeholder="Folder name"
                placeholderTextColor="#aaa"
                autoCapitalize="none"
                autoCorrect={false}
                autoFocus
              />
              <View style={pickerStyles.modalActions}>
                <TouchableOpacity
                  style={pickerStyles.modalCancel}
                  onPress={() => {
                    setMkdirOpen(false);
                    setMkdirName('');
                  }}
                  disabled={mkdirBusy}>
                  <Text style={pickerStyles.modalCancelText}>Cancel</Text>
                </TouchableOpacity>
                <TouchableOpacity
                  style={[pickerStyles.modalSubmit, mkdirBusy && pickerStyles.modalSubmitDisabled]}
                  onPress={submitMkdir}
                  disabled={mkdirBusy || !mkdirName.trim()}>
                  {mkdirBusy ? (
                    <ActivityIndicator color="#fff" />
                  ) : (
                    <Text style={pickerStyles.modalSubmitText}>Create</Text>
                  )}
                </TouchableOpacity>
              </View>
            </View>
          </View>
        </Modal>
      </View>
    </Modal>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 20,
    backgroundColor: '#fff',
    justifyContent: 'center',
    alignItems: 'center',
  },
  title: { fontSize: 24, fontWeight: 'bold', marginBottom: 10 },
  subtitle: {
    fontSize: 16,
    color: '#666',
    textAlign: 'center',
    marginBottom: 24,
  },
  destBox: {
    width: '100%',
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#f0f4f8',
    borderRadius: 10,
    padding: 14,
    marginBottom: 20,
    gap: 12,
  },
  destInfo: { flex: 1 },
  destLabel: { fontSize: 12, color: '#666', marginBottom: 4 },
  destPath: { fontSize: 15, fontWeight: '600', color: '#333' },
  destButton: {
    paddingVertical: 8,
    paddingHorizontal: 12,
    backgroundColor: '#fff',
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#007AFF',
  },
  destButtonText: { color: '#007AFF', fontWeight: '600', fontSize: 13 },
  button: {
    backgroundColor: '#007AFF',
    paddingVertical: 15,
    paddingHorizontal: 30,
    borderRadius: 10,
    width: '100%',
    alignItems: 'center',
  },
  buttonDisabled: { backgroundColor: '#A0A0A0' },
  buttonText: { color: '#fff', fontSize: 18, fontWeight: '600' },
  progressContainer: { marginTop: 30, width: '100%' },
  progressText: { textAlign: 'center', marginBottom: 10, fontSize: 14, color: '#333' },
  progressBar: { height: 10, backgroundColor: '#E0E0E0', borderRadius: 5, overflow: 'hidden' },
  progressFill: { height: '100%', backgroundColor: '#4CD964' },
  backButton: { marginTop: 40 },
  backButtonText: { color: '#007AFF', fontSize: 16 },
});

const pickerStyles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#fff', paddingTop: 50 },
  pathBar: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 12,
    paddingVertical: 8,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#ddd',
    gap: 8,
  },
  upButton: { paddingVertical: 6, paddingHorizontal: 10 },
  upButtonText: { color: '#007AFF', fontSize: 16, fontWeight: '600' },
  pathText: { flex: 1, fontSize: 13, color: '#444', fontFamily: 'monospace' },
  mkdirButton: { paddingVertical: 6, paddingHorizontal: 10 },
  mkdirButtonText: { color: '#007AFF', fontSize: 14, fontWeight: '600' },
  center: { flex: 1, justifyContent: 'center', alignItems: 'center', padding: 20 },
  errorText: { color: '#c44', fontSize: 14, textAlign: 'center', marginBottom: 12 },
  retry: {
    backgroundColor: '#007AFF',
    paddingVertical: 10,
    paddingHorizontal: 20,
    borderRadius: 8,
  },
  retryText: { color: '#fff', fontWeight: '600' },
  emptyText: { color: '#888', fontSize: 14 },
  row: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 12,
    paddingHorizontal: 12,
    gap: 12,
  },
  icon: { fontSize: 22 },
  rowName: { flex: 1, fontSize: 15, color: '#333', fontWeight: '500' },
  chev: { fontSize: 22, color: '#bbb' },
  sep: { height: StyleSheet.hairlineWidth, backgroundColor: '#eee', marginLeft: 56 },
  actions: { padding: 12, borderTopWidth: StyleSheet.hairlineWidth, borderTopColor: '#ddd' },
  confirm: {
    backgroundColor: '#34C759',
    padding: 14,
    borderRadius: 8,
    alignItems: 'center',
  },
  confirmText: { color: '#fff', fontWeight: '600', fontSize: 15 },
  modalBackdrop: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.4)',
    justifyContent: 'center',
    alignItems: 'center',
    padding: 20,
  },
  modalCard: {
    backgroundColor: '#fff',
    borderRadius: 12,
    padding: 20,
    width: '100%',
    maxWidth: 360,
  },
  modalTitle: { fontSize: 18, fontWeight: '600', marginBottom: 4 },
  modalHint: { fontSize: 12, color: '#888', marginBottom: 12 },
  modalInput: {
    borderWidth: 1,
    borderColor: '#ddd',
    borderRadius: 8,
    paddingVertical: 8,
    paddingHorizontal: 10,
    fontSize: 15,
    color: '#333',
    marginBottom: 16,
  },
  modalActions: { flexDirection: 'row', gap: 12 },
  modalCancel: {
    flex: 1,
    paddingVertical: 10,
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#ccc',
    alignItems: 'center',
  },
  modalCancelText: { color: '#444', fontWeight: '600' },
  modalSubmit: {
    flex: 1,
    backgroundColor: '#007AFF',
    paddingVertical: 10,
    borderRadius: 8,
    alignItems: 'center',
  },
  modalSubmitDisabled: { opacity: 0.5, backgroundColor: '#A0A0A0' },
  modalSubmitText: { color: '#fff', fontWeight: '600' },
});
