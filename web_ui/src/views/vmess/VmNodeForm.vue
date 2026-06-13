<template>
  <el-dialog :model-value="visible" title="VMess节点配置" width="700px" @close="close">
    <el-form :model="form" label-width="100px" :rules="rules" ref="formEl">
      <el-form-item label="标签" prop="tag">
        <el-input v-model="form.tag" placeholder="如 vmess-1" />
      </el-form-item>
      <el-form-item label="服务器地址" prop="address">
        <el-input v-model="form.address" placeholder="IP或域名" />
      </el-form-item>
      <el-form-item label="端口" prop="port">
        <el-input-number v-model="form.port" :min="1" :max="65535" />
      </el-form-item>
      <el-form-item label="用户ID" prop="user_id">
        <el-input v-model="form.user_id" placeholder="UUID" />
      </el-form-item>
      <el-form-item label="alterId" prop="alter_id">
        <el-input-number v-model="form.alter_id" :min="0" :max="65535" />
      </el-form-item>
      <el-form-item label="加密方式" prop="security">
        <el-select v-model="form.security">
          <el-option label="auto" value="auto" />
          <el-option label="aes-128-gcm" value="aes-128-gcm" />
          <el-option label="chacha20-poly1305" value="chacha20-poly1305" />
          <el-option label="none" value="none" />
          <el-option label="zero" value="zero" />
        </el-select>
      </el-form-item>
      <el-form-item label="传输协议" prop="network">
        <el-select v-model="form.network" @change="onNetworkChange">
          <el-option label="TCP" value="tcp" />
          <el-option label="WebSocket" value="ws" />
          <el-option label="HTTP/2" value="h2" />
          <el-option label="QUIC" value="quic" />
          <el-option label="mKCP" value="kcp" />
        </el-select>
      </el-form-item>

      <!-- TCP config -->
      <template v-if="form.network === 'tcp'">
        <el-form-item label="头部伪装">
          <el-select v-model="form.tcp_config.header_type">
            <el-option label="none" value="none" />
            <el-option label="http" value="http" />
          </el-select>
        </el-form-item>
      </template>

      <!-- WS config -->
      <template v-if="form.network === 'ws'">
        <el-form-item label="路径">
          <el-input v-model="form.ws_config.path" placeholder="/" />
        </el-form-item>
        <el-form-item label="Host">
          <el-input v-model="form.ws_config.host" placeholder="可选" />
        </el-form-item>
      </template>

      <!-- H2 config -->
      <template v-if="form.network === 'h2'">
        <el-form-item label="路径">
          <el-input v-model="form.h2_config.path" placeholder="/" />
        </el-form-item>
        <el-form-item label="Host">
          <el-input v-model="h2HostStr" placeholder="逗号分隔多个host" />
        </el-form-item>
      </template>

      <!-- QUIC config -->
      <template v-if="form.network === 'quic'">
        <el-form-item label="加密方式">
          <el-select v-model="form.quic_config.security">
            <el-option label="none" value="none" />
            <el-option label="aes-128-gcm" value="aes-128-gcm" />
            <el-option label="chacha20-poly1305" value="chacha20-poly1305" />
          </el-select>
        </el-form-item>
        <el-form-item label="密钥">
          <el-input v-model="form.quic_config.key" placeholder="可选" />
        </el-form-item>
        <el-form-item label="头部伪装">
          <el-select v-model="form.quic_config.header_type">
            <el-option label="none" value="none" />
            <el-option label="srtp" value="srtp" />
            <el-option label="utp" value="utp" />
            <el-option label="wechat-video" value="wechat-video" />
            <el-option label="dtls" value="dtls" />
            <el-option label="wireguard" value="wireguard" />
          </el-select>
        </el-form-item>
      </template>

      <!-- KCP config -->
      <template v-if="form.network === 'kcp'">
        <el-form-item label="MTU">
          <el-input-number v-model="form.kcp_config.mtu" :min="576" :max="1500" />
        </el-form-item>
        <el-form-item label="TTI">
          <el-input-number v-model="form.kcp_config.tti" :min="10" :max="100" />
        </el-form-item>
        <el-form-item label="上行容量">
          <el-input-number v-model="form.kcp_config.uplink_capacity" :min="1" :max="65535" />
        </el-form-item>
        <el-form-item label="下行容量">
          <el-input-number v-model="form.kcp_config.downlink_capacity" :min="1" :max="65535" />
        </el-form-item>
        <el-form-item label="拥塞控制">
          <el-switch v-model="form.kcp_config.congestion" />
        </el-form-item>
        <el-form-item label="头部伪装">
          <el-select v-model="form.kcp_config.header_type">
            <el-option label="none" value="none" />
            <el-option label="srtp" value="srtp" />
            <el-option label="utp" value="utp" />
            <el-option label="wechat-video" value="wechat-video" />
            <el-option label="dtls" value="dtls" />
            <el-option label="wireguard" value="wireguard" />
          </el-select>
        </el-form-item>
      </template>

      <el-form-item label="TLS">
        <el-select v-model="form.tls" @change="onTlsChange">
          <el-option label="关闭" value="none" />
          <el-option label="TLS" value="tls" />
        </el-select>
      </el-form-item>

      <template v-if="form.tls === 'tls'">
        <el-form-item label="允许不安全">
          <el-switch v-model="form.tls_config.allow_insecure" />
          <el-text v-if="form.tls_config.allow_insecure" type="danger" style="margin-left:8px">
            开启此选项有安全风险
          </el-text>
        </el-form-item>
        <el-form-item label="SNI">
          <el-input v-model="form.tls_config.server_name" placeholder="可选，服务器名指示" />
        </el-form-item>
      </template>

      <el-form-item label="备注">
        <el-input v-model="form.remark" placeholder="节点备注" maxlength="128" />
      </el-form-item>
    </el-form>
    <template #footer>
      <el-button @click="close">取消</el-button>
      <el-button type="primary" @click="submit">保存</el-button>
    </template>
  </el-dialog>
</template>

<script setup lang="ts">
import { ref, computed } from 'vue'
import { ElMessage } from 'element-plus'
import type { FormInstance } from 'element-plus'
import request from '../../utils/request'

const emit = defineEmits(['saved'])
const visible = ref(false)
const isEdit = ref(false)
const editTag = ref('')
const formEl = ref<FormInstance>()

const defaultForm = () => ({
  tag: '', address: '', port: 443, user_id: '', alter_id: 0,
  security: 'auto', network: 'tcp', tls: 'none', remark: '',
  tcp_config: { header_type: 'none' },
  ws_config: { path: '/', host: '' },
  h2_config: { path: '/', host: [] as string[] },
  quic_config: { security: 'none', key: '', header_type: 'none' },
  kcp_config: { mtu: 1350, tti: 20, uplink_capacity: 5, downlink_capacity: 20, congestion: false, header_type: 'none' },
  tls_config: { allow_insecure: false, server_name: '' }
})

const form = ref(defaultForm())

const h2HostStr = computed({
  get: () => form.value.h2_config.host.join(','),
  set: (v: string) => { form.value.h2_config.host = v ? v.split(',').map(s => s.trim()).filter(Boolean) : [] }
})

const rules = {
  tag: [{ required: true, message: '请输入标签', trigger: 'blur' }],
  address: [{ required: true, message: '请输入服务器地址', trigger: 'blur' }],
  port: [{ required: true, message: '请输入端口', trigger: 'blur' }],
  user_id: [{ required: true, message: '请输入UUID', trigger: 'blur' }]
}

function open(data: any) {
  isEdit.value = !!data?.tag
  editTag.value = data?.tag || ''
  if (data?.tag) {
    form.value = { ...defaultForm(), ...data }
  } else {
    form.value = defaultForm()
  }
  visible.value = true
}

function close() { visible.value = false }

function onNetworkChange() {}
function onTlsChange() {}

async function submit() {
  try {
    if (isEdit.value) {
      await request.put(`/api/v2rayn/vmess/nodes/${editTag.value}`, form.value)
    } else {
      await request.post('/api/v2rayn/vmess/nodes', form.value)
    }
    ElMessage.success('保存成功')
    close()
    emit('saved')
  } catch (e: any) {
    ElMessage.error(e?.response?.data?.error || '保存失败')
  }
}

defineExpose({ open })
</script>
